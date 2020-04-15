#define coulombConstant 8.99 * pow(10.0f, 9.0f)
#define chargeProton    1.60217662 * pow(10.0f, -19.0f)
#define chargeElectron  -chargeProton
#define massProton      1.67262190 * pow(10.0f, -27.0f)
#define massElectron    9.10938356 * pow(10.0f, -31.0f)
#define proton          1.0f
#define electron        2.0f

//-----------------------------------------------------------------------------
// Vec3 Helpers
//-----------------------------------------------------------------------------

float magnitude(float3 vector) {
	return sqrt(
		pow(vector.x, 2.0f) + 
		pow(vector.y, 2.0f) + 
		pow(vector.z, 2.0f)
	);
}

float3 normal(float3 vector) {
	float factor = 1.0 / magnitude(vector);
	return vector * factor;
}

//-----------------------------------------------------------------------------
// Particle Helpers
//-----------------------------------------------------------------------------

float getMass(float type) {
	if (type == proton) {
		return massProton;
	}
	else if (type == electron) {
		return massElectron;
	}
	else {
		return 0.0f;
	}
}

float getCharge(float type) {
	if (type == proton) {
		return chargeProton;
	}
	else if (type == electron) {
		return chargeElectron;
	}
	else {
		return 0.0f;
	}
}

float3 computeForceOnMe(int my_index, int other_index, __global float4 *particlePositions) {
	if (particlePositions[my_index].w == proton) {
		 return (float3)(0.0f, 0.0f, 0.0f);
	}

	if (my_index == other_index) {
		 return (float3)(0.0f, 0.0f, 0.0f);
	}

	float3 direction = particlePositions[my_index].xyz - particlePositions[other_index].xyz;
	float q1 = getCharge(particlePositions[my_index].w);
	float q2 = getCharge(particlePositions[other_index].w);
	float r = magnitude(direction);
	return normal(direction) * (float)(coulombConstant * q1 * q2 / pow(r, 2.0f));
}
 
//-----------------------------------------------------------------------------
// Kernel
//-----------------------------------------------------------------------------

// Compute k0 or k1
__kernel void computeForces(__global float3 *particleForces, 
 							 __global float4 *particlePositions) 
{
	int gid = get_global_id(0);
	float3 totalForces = (float3)(0.0f, 0.0f, 0.0f);
	
	for (int i = 0; i < NUM_PARTICLES; i++) {
		totalForces += computeForceOnMe(gid, i, particlePositions);
	}

	particleForces[gid] = totalForces;
}
 
// Compute y1
__kernel void computeApproxPositions(__global float *timeStep,
									 __global float3 *particleForces,
									 __global float4 *particlePositions0,
									 __global float4 *particlePositions1) 
{
	int gid = get_global_id(0);
	float h = *timeStep;

	float mass = getMass(particlePositions0[gid].w);
	float3 f = particleForces[gid];
	float3 deltaDist = f * pow(h, 2) / mass;
	particlePositions1[gid].xyz = particlePositions0[gid].xyz + deltaDist;
	particlePositions1[gid].w = particlePositions0[gid].w;
}

// Compute z1 and check if error is acceptable
__kernel void computeBetterPositionsAndCheckError(
	__global float *timeStep,
	__global float3 *particleForces0,
	__global float3 *particleForces1,
	__global float4 *particlePositions0, // y0
	__global float4 *particlePositions1, // y1
	__global float4 *particlePositions2, // z1
	__global int *errorAcceptableFlag) 
{
	int gid = get_global_id(0);
	float h = *timeStep;

	float mass = getMass(particlePositions0[gid].w);
	float3 f0 = particleForces0[gid];
	float3 f1 = particleForces1[gid];

	float3 avgForce = (f0 + f1) / 2.0f;
	float3 deltaDist = avgForce * pow(h, 2) / mass;
	particlePositions2[gid].xyz = particlePositions0[gid].xyz + deltaDist;
	particlePositions2[gid].w = particlePositions0[gid].w;

	float error = magnitude(particlePositions2[gid].xyz - particlePositions1[gid].xyz);

	if (error > ERROR_TOLERANCE) {
		*errorAcceptableFlag = 0;
	}
}
