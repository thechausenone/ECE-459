#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>

#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <base64/base64.h>

using std::cout;
using std::cerr;
using std::endl;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

int gMaxSecretLen = 4;

std::string gAlphabet = "abcdefghijklmnopqrstuvwxyz"
                        "0123456789";

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

void usage(const char *cmd) {
    cout << cmd << " <token> [maxLen] [alphabet]" << endl;
    cout << endl;

    cout << "Defaults:" << endl;
    cout << "maxLen = " << gMaxSecretLen << endl;
    cout << "alphabet = " << gAlphabet << endl;
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::stringstream jwt;
    jwt << argv[1];

    if (argc > 2) {
        gMaxSecretLen = atoi(argv[2]);
    }
    if (argc > 3) {
        gAlphabet = argv[3];
    }

    std::string header64;
    getline(jwt, header64, '.');

    std::string payload64;
    getline(jwt, payload64, '.');

    std::string origSig64;
    getline(jwt, origSig64, '.');

    // Our goal is to find the secret to HMAC this string into our origSig
    std::string message = header64 + '.' + payload64;
    std::string origSig = base64_decode(origSig64);

    // Setup other variables
    int earlyTermination = 0;
    int *earlyTerminationFlag = &earlyTermination;

    // Use OpenCL to brute force JWT
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
        std::ifstream sourceFile("src/jwtcracker.cl");
            if(!sourceFile.is_open()){
                std::cerr << "Cannot find kernel file" << std::endl;
                throw;
            }
        std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
        cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length() + 1));
 
        // Make program of the source code in the context
        cl::Program program = cl::Program(context, source);
        
        // Setup build options
        char buildOptions[120];
        sprintf(
            buildOptions, "-DMESSAGE_LENGTH=%d -DMAX_SECRET_LENGTH=%d -DALPHABET_LENGTH=%d -DMAX_MB_LENGTH=%d", 
            (int)message.length(), gMaxSecretLen, (int)gAlphabet.length(), (int)(message.length() + (512/8))
        );

        // Build program for these specific devices
        try {
            program.build(devices, buildOptions);
        } catch(cl::Error error) {
            std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]) << std::endl;
            throw;
        }
 
        // Make kernels
        cl::Kernel kernel(program, "bruteForceJWT");

        // Create buffers
        cl::Buffer messageBuffer = cl::Buffer(
            context, 
            CL_MEM_READ_ONLY, 
            sizeof(char) * message.length()
        );
        cl::Buffer origSigBuffer = cl::Buffer(
            context, 
            CL_MEM_READ_ONLY, 
            sizeof(char) * origSig.length()
        );
        cl::Buffer alphabetBuffer = cl::Buffer(
            context, 
            CL_MEM_READ_ONLY, 
            sizeof(char) * gAlphabet.length()
        );
        cl::Buffer earlyTerminationBuffer = cl::Buffer(
            context, 
            CL_MEM_READ_WRITE, 
            sizeof(int)
        );

        // Write buffers
        queue.enqueueWriteBuffer(
            messageBuffer, 
            CL_TRUE, 
            0, 
            sizeof(char) * message.length(), 
            (char*)message.c_str()
        );
        queue.enqueueWriteBuffer(
            origSigBuffer, 
            CL_TRUE, 
            0, 
            sizeof(char) * origSig.length(), 
            (char*)origSig.c_str()
        );                
        queue.enqueueWriteBuffer(
            alphabetBuffer, 
            CL_TRUE, 
            0, 
            sizeof(char) * gAlphabet.length(), 
            (char*)gAlphabet.c_str()
        );
        queue.enqueueWriteBuffer(
            earlyTerminationBuffer, 
            CL_TRUE, 
            0, 
            sizeof(int),
            earlyTerminationFlag
        );

        // Set arguments to kernel
        kernel.setArg(0, messageBuffer);
        kernel.setArg(1, origSigBuffer);
        kernel.setArg(2, alphabetBuffer);
        kernel.setArg(3, earlyTerminationBuffer);

        // Run the kernel on specific ND range
        cl::NDRange globalSize(gAlphabet.length(), gAlphabet.length(), gAlphabet.length());
        queue.enqueueNDRangeKernel(kernel, cl::NullRange, globalSize); 

    } catch(cl::Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
    }

    return 0;
}
