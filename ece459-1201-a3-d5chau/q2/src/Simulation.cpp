#include "Simulation.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

using std::cerr;
using std::cout;
using std::endl;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

bool retIsEmpty(const std::vector<Particle*> &ret) {
    for (Particle* p : ret) {
        if (p != nullptr) {
            return false;
        }
    }

    return true;
}

void reset(std::vector<Particle*> &v) {
    for (int i = 0; i < v.size(); i++) {
        delete v[i];
        v[i] = nullptr;
    }
}

//-----------------------------------------------------------------------------
// Simulation
//-----------------------------------------------------------------------------

Simulation::Simulation(float initTimeStep, float errorTolerance)
    : initTimeStep(initTimeStep)
    , errorTolerance(errorTolerance) {
    // nop
}

Simulation::~Simulation() {
    reset(y0);
    reset(y1);
    reset(z1);
}

//-----------------------------------------------------------------------------
// Input/output
//-----------------------------------------------------------------------------

void Simulation::readInputFile(std::string inputFile) {
    std::ifstream file(inputFile);
    if (!file.is_open()) {
        cerr << "Unable to open file: " << inputFile << endl;
        exit(1);
    }

    std::string line;
    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string token;

        getline(ss, token, ',');
        Particle::ParticleType type = (token == "p" ? Particle::PROTON : Particle::ELECTRON);

        getline(ss, token, ',');
        float x = std::stod(token);

        getline(ss, token, ',');
        float y = std::stod(token);

        getline(ss, token, ',');
        float z = std::stod(token);

        y0.push_back(new Particle(
            type,
            Vec3(x, y, z)
        ));
    }
}

void Simulation::print() {
    int digitsAfterDecimal = 5;
    int width = digitsAfterDecimal + std::string("-0.e+00").length();

    for (const Particle* p : z1) {
        char type;
        switch (p->type) {
            case Particle::PROTON:
                type = 'p';
                break;
            case Particle::ELECTRON:
                type = 'e';
                break;
            default:
                type = 'u';
        }

        cout << type << ","
             << std::scientific
             << std::setprecision(digitsAfterDecimal)
             << std::setw(width)
             << p->position.x << ","
             << std::setw(width)
             << p->position.y << ","
             << std::setw(width)
             << p->position.z << endl;
    }
}

//-----------------------------------------------------------------------------
// OpenCL Simulation
//-----------------------------------------------------------------------------

#ifdef USE_OPENCL

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

void Simulation::run() {
    const int numParticles = y0.size();
    cl_float4 y0Converted[numParticles];
    float h = initTimeStep;

    // Convert y0 into a float4 array
    for (int i = 0; i < numParticles; i++) {
        y0Converted[i] = (cl_float4){ 
            y0[i]->position.x, 
            y0[i]->position.y, 
            y0[i]->position.z, 
            (float)y0[i]->type
        };
    }

    try { 
        // Get available platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);

        // Select the default platform and create a context using this platform and the GPU
        cl_context_properties cps[3] = { 
            CL_CONTEXT_PLATFORM, 
            (cl_context_properties)(platforms[0])(), 
            0 
        };
        cl::Context context(CL_DEVICE_TYPE_GPU, cps);
 
        // Get a list of devices on this platform
        std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
 
        // Create a command queue and use the first device
        cl::CommandQueue queue = cl::CommandQueue(context, devices[0]);
 
        // Read source file
        std::ifstream sourceFile("src/protons.cl");
            if(!sourceFile.is_open()){
                std::cerr << "Cannot find kernel file" << std::endl;
                throw;
            }
        std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
        cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()+1));
 
        // Make program of the source code in the context
        cl::Program program = cl::Program(context, source);

        // Setup build options
        char buildOptions[364];
        sprintf(
            buildOptions, 
            "-DNUM_PARTICLES=%d -DERROR_TOLERANCE=%f", 
            numParticles,
            errorTolerance
        );

        // Build program for these specific devices
        try {
            program.build(devices, buildOptions);
        } catch(cl::Error error) {
            std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]) << std::endl;
            throw;
        }
 
        // Make kernels
        cl::Kernel kernelComputeForces(program, "computeForces");
        cl::Kernel kernelComputeApproxPositions(program, "computeApproxPositions");
        cl::Kernel kernelComputeBetterPositionsAndCheckError(program, "computeBetterPositionsAndCheckError");

        // Create buffers
        cl::Buffer hBuffer(
            context,
            CL_MEM_READ_ONLY,
            sizeof(float)
        );
        cl::Buffer errorAcceptableFlagBuffer(
            context,
            CL_MEM_READ_WRITE,
            sizeof(int)
        );
        cl::Buffer y0Buffer(
            context,
            CL_MEM_READ_ONLY,
            numParticles * sizeof(cl_float4)
        );
        cl::Buffer y1Buffer(
            context,
            CL_MEM_READ_WRITE,
            numParticles * sizeof(cl_float4)
        );
        cl::Buffer z1Buffer(
            context,
            CL_MEM_READ_WRITE,
            numParticles * sizeof(cl_float4)
        );
        cl::Buffer k0Buffer(
            context,
            CL_MEM_READ_WRITE,
            numParticles * sizeof(cl_float3)
        );
        cl::Buffer k1Buffer(
            context,
            CL_MEM_READ_WRITE,
            numParticles * sizeof(cl_float3)
        );

        // Write buffers
        queue.enqueueWriteBuffer(
            y0Buffer,
            CL_TRUE,
            0,
            numParticles * sizeof(cl_float4),
            y0Converted
        );

        // Set arguments to kernel
        kernelComputeForces.setArg(0, k0Buffer);
        kernelComputeForces.setArg(1, y0Buffer);

        kernelComputeApproxPositions.setArg(0, hBuffer);
        kernelComputeApproxPositions.setArg(1, k0Buffer);
        kernelComputeApproxPositions.setArg(2, y0Buffer);
        kernelComputeApproxPositions.setArg(3, y1Buffer);

        kernelComputeBetterPositionsAndCheckError.setArg(0, hBuffer);
        kernelComputeBetterPositionsAndCheckError.setArg(1, k0Buffer);
        kernelComputeBetterPositionsAndCheckError.setArg(2, k1Buffer);
        kernelComputeBetterPositionsAndCheckError.setArg(3, y0Buffer);
        kernelComputeBetterPositionsAndCheckError.setArg(4, y1Buffer);
        kernelComputeBetterPositionsAndCheckError.setArg(5, z1Buffer);
        kernelComputeBetterPositionsAndCheckError.setArg(6, errorAcceptableFlagBuffer);

        // Run the kernels on specific ND range
        cl::NDRange globalSize(numParticles);

        // Compute k0
        queue.enqueueNDRangeKernel(
            kernelComputeForces, 
            cl::NullRange, 
            globalSize
        );

        // Reassign kernel arguments to compute k1 later
        kernelComputeForces.setArg(0, k1Buffer); 
        kernelComputeForces.setArg(1, y1Buffer);

        while (true) {
            int errorAcceptableFlag = 1;

            // Update value of timestep from prev iteration
            queue.enqueueWriteBuffer(
                hBuffer,
                CL_TRUE,
                0,
                sizeof(float),
                &h
            );

            // Compute y1
            queue.enqueueNDRangeKernel(
                kernelComputeApproxPositions, 
                cl::NullRange, 
                globalSize
            );

            // Compute k1
            queue.enqueueNDRangeKernel(
                kernelComputeForces, 
                cl::NullRange, 
                globalSize
            );

            // Compute z1 and check error
            queue.enqueueWriteBuffer(
                errorAcceptableFlagBuffer,
                CL_TRUE,
                0,
                sizeof(int),
                &errorAcceptableFlag
            );
            queue.enqueueNDRangeKernel(
                kernelComputeBetterPositionsAndCheckError, 
                cl::NullRange, 
                globalSize
            );
            queue.enqueueReadBuffer(
                errorAcceptableFlagBuffer,
                CL_TRUE,
                0,
                sizeof(int),
                &errorAcceptableFlag
            );

            // Check if error was acceptable
            if (errorAcceptableFlag == 1) {
                break;
            }

            h = h / 2.0f;
        }

        // Read z1 out from its float4 format
        cl_float4 z1Converted[numParticles];
        queue.enqueueReadBuffer(
            z1Buffer,
            CL_TRUE,
            0,
            numParticles * sizeof(cl_float4),
            z1Converted
        );

        // Convert z1 from float4 format to Particle format
        z1.clear();
        for (int i = 0; i < numParticles; i++) {
            Particle::ParticleType particleType;
            if (z1Converted[i].s[3] == 1.0f) {
                particleType = Particle::PROTON;
            }
            else if (z1Converted[i].s[3] == 2.0f) {
                particleType = Particle::ELECTRON;
            }
            else {
                particleType = Particle::UNDEFINED;
            }
            Particle *particle = new Particle(
                particleType,
                Vec3(
                    z1Converted[i].s[0],
                    z1Converted[i].s[1],
                    z1Converted[i].s[2]
                )
            );
            z1.push_back(particle);
        }

    } catch(cl::Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
    }
}

// ifdef USE_OPENCL
#endif

//-----------------------------------------------------------------------------
// CPU Simulation
//-----------------------------------------------------------------------------

#ifndef USE_OPENCL

void Simulation::computeForces(std::vector<Vec3> &ret, const std::vector<Particle*> &particles) {
    assert(ret.size() == 0);
    ret.resize(particles.size());

    #pragma omp parallel for
    for (int i = 0; i < particles.size(); i++) {
        Vec3 totalForces;

        for (int j = 0; j < particles.size(); j++) {
            totalForces += particles[i]->computeForceOnMe(particles[j]);
        }

        ret[i] = totalForces;
    }

    assert(ret.size() == particles.size());
}

void Simulation::computeApproxPositions(const float h) {
    assert(y0.size() == k0.size());
    assert(retIsEmpty(y1));

    #pragma omp parallel for
    for (int i = 0; i < y0.size(); i++) {
        float mass = y0[i]->getMass();
        Vec3 f = k0[i];

        // h's unit is in seconds
        //
        //         F = ma
        //     F / m = a
        //   h F / m = v
        // h^2 F / m = d

        Vec3 deltaDist = f * std::pow(h, 2) / mass;
        y1[i] = new Particle(y0[i]->type, y0[i]->position + deltaDist);
    }
}

void Simulation::computeBetterPositions(const float h) {
    assert(y0.size() == k0.size());
    assert(y0.size() == k1.size());
    assert(retIsEmpty(z1));

    #pragma omp parallel for
    for (int i = 0; i < y0.size(); i++) {
        float mass = y0[i]->getMass();
        Vec3 f0 = k0[i];
        Vec3 f1 = k1[i];

        Vec3 avgForce = (f0 + f1) / 2.0;
        Vec3 deltaDist = avgForce * std::pow(h, 2) / mass;
        Vec3 y1 = y0[i]->position + deltaDist;

        z1[i] = new Particle(y0[i]->type, y1);
    }
}

bool Simulation::isErrorAcceptable(const std::vector<Particle*> &p0, const std::vector<Particle*> &p1) {
    assert(p0.size() == p1.size());

    bool errorAcceptable = true;

    #pragma omp parallel for
    for (int i = 0; i < p0.size(); i++) {
        if ((p0[i]->position - p1[i]->position).magnitude() > errorTolerance) {
            #pragma omp critical
            {
                errorAcceptable = false;
            }
        }
    }

    return errorAcceptable;
}

void Simulation::run() {
    const int numParticles = y0.size();
    k0.reserve(numParticles);
    k1.reserve(numParticles);
    y1.resize(numParticles);
    z1.resize(numParticles);

    float h = initTimeStep;

    k0.clear();
    computeForces(k0, y0); // Compute k0

    while (true) {
        k1.clear();
        reset(y1);
        reset(z1);

        computeApproxPositions(h); // Compute y1
        computeForces(k1, y1); // Compute k1
        computeBetterPositions(h); // Compute z1

        if (isErrorAcceptable(z1, y1)) {
            // Error is acceptable so we can stop simulation
            break;
        }

        h /= 2.0;
    }
}

// ifndef USE_OPENCL
#endif
