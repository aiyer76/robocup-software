#include "GL_ShapeDrawer.h"
#include "SimpleVehicle.hpp"
#include <physics/SimEngine.hpp>
#include <physics/PhysicsConstants.hpp>
#include <iostream>

#define CUBE_HALF_EXTENTS 1

using namespace std;

//FIXME: parameters have no sensible interpretation
// static physics parameters
static const float maxEngineForce = 3000.f; //this should be engine/velocity dependent
static const float maxBreakingForce = 100.f;
static const float steeringIncrement = 0.04f;
static const float steeringClamp = 0.3f;
static const float wheelRadius = 0.5f/2.f;
static const float wheelWidth = 0.4f/2.f;
static const float wheelFriction = 1000; //BT_LARGE_FLOAT;
static const float suspensionStiffness = 1000.f;
static const float suspensionDamping = 2.3f;
static const float suspensionCompression = 4.4f;
static const float rollInfluence = 0.0f;
static const btScalar suspensionRestLength = 0.5f;//0.6f

static const btScalar maxVelocity = 10; //m/s? unused


void SimpleVehicle::initPhysics() {
	// Assumes that Y is up
	int rightIndex = 0;
	int upIndex = 1;
	int forwardIndex = 2;
	btVector3 wheelDirectionCS0(0, -1, 0);
	btVector3 wheelAxleCS;


	//Robot chassis
	//TODO: use convexhull for robot shape
	btCylinderShape* cShape = new btCylinderShape(btVector3(Sim_Robot_Radius, Sim_Robot_Height/2.f, Sim_Robot_Radius));
	//cShape->setMargin(btScalar(0.04));

	btCollisionShape* chassisShape = cShape;
	_simEngine->addCollisionShape(chassisShape);

	btCompoundShape* compound = new btCompoundShape();
	_simEngine->addCollisionShape(compound);
	btTransform localTrans;
	localTrans.setIdentity();
	//localTrans effectively shifts the center of mass with respect to the chassis
	localTrans.setOrigin(btVector3(0, 0, 0));

	compound->addChildShape(localTrans, chassisShape);

	btTransform vehicleTr;
	vehicleTr.setIdentity();
	float connectionHeight = -0.1f;//Wheel connection height
	vehicleTr.setOrigin(btVector3(0, Sim_Robot_Height/2.f-connectionHeight, 0));//spawn location in WS

	_carChassis = _simEngine->localCreateRigidBody(800, vehicleTr, compound);
	//_carChassis->setDamping(0.2,0.2);

	_wheelShape = new btCylinderShapeX(
			btVector3(Sim_Wheel_Width/2.f, Sim_Wheel_Radius, Sim_Wheel_Radius));

	resetScene(); // force initial physics state - everything stationary

	/// create vehicle
	{

		_vehicleRayCaster = new btDefaultVehicleRaycaster(_simEngine->dynamicsWorld());
		_vehicle = new btRaycastVehicle(_tuning, _carChassis, _vehicleRayCaster);

		///never deactivate the vehicle
		_carChassis->setActivationState(DISABLE_DEACTIVATION);

		_simEngine->addVehicle(_vehicle);

		bool isFrontWheel = false;

		//choose coordinate system
		_vehicle->setCoordinateSystem(rightIndex, upIndex, forwardIndex);


		///Omni-wheels

		//FL
		btVector3 connectionPointCS0 = btVector3(0,connectionHeight,Sim_Robot_Radius-Sim_Wheel_Width/2.f).rotate(btVector3(0,1,0),3.141/4.f*1);
		wheelAxleCS = btVector3(0,0,1).rotate(btVector3(0,1,0), 3.141/4.f*1);
		_vehicle->addWheel(connectionPointCS0, wheelDirectionCS0, wheelAxleCS,
				suspensionRestLength, wheelRadius, _tuning, isFrontWheel);

		//FR
		connectionPointCS0 = btVector3(0,connectionHeight,Sim_Robot_Radius-Sim_Wheel_Width/2.f).rotate(btVector3(0,1,0),3.141/4.f*-1);
		wheelAxleCS = btVector3(0,0,1).rotate(btVector3(0,1,0), 3.141/4.f*-1);
		_vehicle->addWheel(connectionPointCS0, wheelDirectionCS0, wheelAxleCS,
				suspensionRestLength, wheelRadius, _tuning, isFrontWheel);

		//BR
		connectionPointCS0 = btVector3(0,connectionHeight,Sim_Robot_Radius-Sim_Wheel_Width/2.f).rotate(btVector3(0,1,0),3.141/4.f*5);
				wheelAxleCS = btVector3(0,0,1).rotate(btVector3(0,1,0), 3.141/4.f*5);
		isFrontWheel = false;
		_vehicle->addWheel(connectionPointCS0, wheelDirectionCS0, wheelAxleCS ,
				suspensionRestLength, wheelRadius, _tuning, isFrontWheel);

		//BL
		connectionPointCS0 = btVector3(0,connectionHeight,Sim_Robot_Radius-Sim_Wheel_Width/2.f).rotate(btVector3(0,1,0),3.141/4.f*3);
				wheelAxleCS = btVector3(0,0,1).rotate(btVector3(0,1,0), 3.141/4.f*3);
		_vehicle->addWheel(connectionPointCS0, wheelDirectionCS0, wheelAxleCS,
				suspensionRestLength, wheelRadius, _tuning, isFrontWheel);

		for (int i = 0; i < _vehicle->getNumWheels(); i++) {
			btWheelInfo& wheel = _vehicle->getWheelInfo(i);
			wheel.m_suspensionStiffness = suspensionStiffness;
			wheel.m_wheelsDampingRelaxation = suspensionDamping;
			wheel.m_wheelsDampingCompression = suspensionCompression;
			wheel.m_frictionSlip = wheelFriction;
			wheel.m_rollInfluence = rollInfluence;
		}
	}
}

void SimpleVehicle::steerLeft() {
	engineForce(maxEngineForce);
	_breakingForce = 0.f;
}

void SimpleVehicle::steerRight() {
	engineForce(-maxEngineForce);
	_breakingForce = 0.f;
}

void SimpleVehicle::driveForward() {
	_engineForce[0] = -maxEngineForce;
	_engineForce[1] = maxEngineForce;
	_engineForce[2] = maxEngineForce;
	_engineForce[3] = -maxEngineForce;
	_breakingForce = 0.f;
}

void SimpleVehicle::driveBackward() {
	_breakingForce = maxBreakingForce;
	engineForce(0.f);
}

void SimpleVehicle::move() {
	int wheelIndex = 2;
	_vehicle->applyEngineForce(_engineForce[wheelIndex], wheelIndex);
	_vehicle->setBrake(_breakingForce, wheelIndex);
	wheelIndex = 3;
	_vehicle->applyEngineForce(_engineForce[wheelIndex], wheelIndex);
	_vehicle->setBrake(_breakingForce, wheelIndex);

	wheelIndex = 0;
	_vehicle->applyEngineForce(_engineForce[wheelIndex], wheelIndex);
	_vehicle->setBrake(_breakingForce, wheelIndex);
	wheelIndex = 1;
	_vehicle->applyEngineForce(_engineForce[wheelIndex], wheelIndex);
	_vehicle->setBrake(_breakingForce, wheelIndex);
}

void SimpleVehicle::drawWheels(GL_ShapeDrawer* shapeDrawer, const btVector3& worldBoundsMin, const btVector3& worldBoundsMax) {
	btVector3 wheelColor(1, 0, 0);
	btScalar m[16];
	for (int i = 0; i < _vehicle->getNumWheels(); i++) {
		//synchronize the wheels with the (interpolated) chassis worldtransform
		_vehicle->updateWheelTransform(i, true);
		//draw wheels (cylinders)
		_vehicle->getWheelInfo(i).m_worldTransform.getOpenGLMatrix(m);
		int debug_mode = _simEngine->dynamicsWorld()->getDebugDrawer()->getDebugMode();
		shapeDrawer->drawOpenGL(m, _wheelShape, wheelColor, debug_mode,
				worldBoundsMin, worldBoundsMax);
	}
}

void SimpleVehicle::resetScene() {
	engineForce(0);
	_vehicleSteering = 0.f;
	_carChassis->setCenterOfMassTransform(((btDefaultMotionState* ) _carChassis->getMotionState())->m_startWorldTrans);
	_carChassis->setLinearVelocity(btVector3(0, 0, 0));
	_carChassis->setAngularVelocity(btVector3(0, 0, 0));
	_simEngine->dynamicsWorld()->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
			_carChassis->getBroadphaseHandle(), _simEngine->dynamicsWorld()->getDispatcher());
	if (_vehicle) {
		_vehicle->resetSuspension();
		for (int i = 0; i < _vehicle->getNumWheels(); i++) {
			//synchronize the wheels with the (interpolated) chassis worldtransform
			_vehicle->updateWheelTransform(i, true);
		}
	}
}

void SimpleVehicle::getWorldTransform(btTransform& chassisWorldTrans) const {
	_carChassis->getMotionState()->getWorldTransform(chassisWorldTrans);
}


