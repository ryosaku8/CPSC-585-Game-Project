#include "AIMind.h"


AIMind::AIMind(Racer* _racer, TypeOfRacer _racerType)
{
	numberOfLapsToWin = 3;
	racer = _racer;
	racerType = _racerType;
	newTime = NULL;
	oldTime = time(NULL);
	lastPosition = racer->body->getPosition();
	currentWaypoint = 0;
	currentLap = 1;
	overallPosition = 0;
	placement = 1;
	checkPointTimer = new CheckpointTimer(racer);
	speedBoost = new Ability(SPEED);
	laser = new Ability(LASER);
	rocket = new Ability(ROCKET);
	landmine = new Ability(LANDMINE);
	knownNumberOfKills = 0;
	rotationAngle = 0;
}

AIMind::~AIMind(void)
{
	if (checkPointTimer)
	{
		delete checkPointTimer;
		checkPointTimer = NULL;
	}

	if (speedBoost)
	{
		delete speedBoost;
		speedBoost = NULL;
	}

	if (laser)
	{
		delete laser;
		laser = NULL;
	}

	if (rocket)
	{
		delete rocket;
		rocket = NULL;
	}

	if (landmine)
	{
		delete landmine;
		landmine = NULL;
	}
}

void AIMind::update(HUD* hud, Intention intention, float seconds, Waypoint* waypoints[], Waypoint* checkpoints[], Waypoint* prevCheckpoints[], Racer* racers[]){
	// Once the race is completed, the player is turned into an AI at which point an end of game hud would display.
	if(currentLap == numberOfLapsToWin+1){ 
		//Possibly add a value here that means the racer has completed the race. Like raceCompleted = true;
		if(racerType == PLAYER){
			togglePlayerComputerAI();
		}
	}

	checkPointTime = checkPointTimer->update(checkpoints, prevCheckpoints);

	updateWaypointsAndLap(seconds, waypoints);

	if(checkPointTimer->downgradeAbility()){
		downgrade();
	}

	if(racer->kills > knownNumberOfKills){
		knownNumberOfKills += 1;
		upgrade();
	}


	switch(racerType){
		case PLAYER:
			{
				// Update camera
				if (!intention.lbumpPressed)
				{
					if ((intention.cameraX != 0) || (intention.cameraY != 0))
					{
						hkReal angle;
						float height;

						angle = intention.cameraX * 0.05f;

						if (racer->config.inverse)
							height = intention.cameraY * -0.02f + racer->lookHeight;
						else
							height = intention.cameraY * 0.02f + racer->lookHeight;

						if (height > 0.5f)
							height = 0.5f;
						else if (height < -0.5f)
							height = -0.5f;

						racer->lookHeight = height;

						if (angle > M_PI)
							angle = (hkReal) M_PI;
						else if (angle < -M_PI)
							angle = (hkReal) -M_PI;


						hkQuaternion rotation;

						if (angle < 0.0f)
						{
							angle *= -1;
							rotation.setAxisAngle(hkVector4(0,-1,0), angle);
						}
						else
						{
							rotation.setAxisAngle(hkVector4(0,1,0), angle);
						}

						hkTransform transRot;
						transRot.setIdentity();
						transRot.setRotation(rotation);

						hkVector4 finalLookDir(0,0,1);
						finalLookDir.setTransformedPos(transRot, racer->lookDir);

						finalLookDir(1) = height;

						racer->lookDir.setXYZ(finalLookDir);
					}
				}

				// Update Heads Up Display
				hud->update(intention);

				hkVector4 vel = racer->body->getLinearVelocity();
				float velocity = vel.dot3(racer->drawable->getZhkVector());

				hud->setSpeed(velocity);
				hud->setHealth(racer->health);
				hud->setPosition(placement);
				hud->setLap(currentLap, numberOfLapsToWin);

				racer->computeRPM();

				racer->braking = false;

				if (intention.leftTrig && ((hkMath::abs(velocity) > 0.1f) ||
					 (hkMath::abs((racer->body->getAngularVelocity()).dot3(racer->body->getAngularVelocity())) > 0.1f)))
				{
					racer->brake(seconds);
				}


				if(hud->getSelectedAbility() == SPEED && intention.rightTrig && !speedBoost->onCooldown()){
					speedBoost->startCooldownTimer();
					Sound::sound->playBoost(racer->emitter);
				}

				if(hud->getSelectedAbility() == LASER && intention.rightTrig && !laser->onCooldown()){
					laser->startCooldownTimer();
					racer->fireLaser();
				}

				if (hud->getSelectedAbility() == ROCKET && intention.rightTrig && !rocket->onCooldown())
				{
					rocket->startCooldownTimer();
					racer->fireRocket();
				}

				if (hud->getSelectedAbility() == LANDMINE && intention.rightTrig && !landmine->onCooldown())
				{
					landmine->startCooldownTimer();
					racer->dropMine();
				}


				if(speedBoost->onCooldown()){
					char buf1[33];
					speedBoost->updateCooldown(seconds);
					std::string stringArray[] = { buf1 };//, buf2, buf3, buf4 };
					//renderer->setText(stringArray, 1);
				}

				if(laser->onCooldown()){
					laser->updateCooldown(seconds);
				}

				if(rocket->onCooldown()){
					rocket->updateCooldown(seconds);
				}

				if(landmine->onCooldown()){
					landmine->updateCooldown(seconds);
				}

				/************* STEERING CALCULATIONS *************/
				hkVector4 A = racer->drawable->getZhkVector();
				hkVector4 C = racer->body->getPosition();
				hkVector4 B;
				B.setXYZ(racer->lookDir);
				B(1) = 0.0f;

				float angle = acos(B.dot3(A));

				float sign = B.dot3(racer->drawable->getXhkVector());

				if (racer->currentAcceleration < 0.0f)
					sign *= -1.0f;

				if ((angle > 0.0f) && (sign > 0))
				{
					racer->steer(seconds, min(angle / 1.11f, 1.0f));
				}
				else if ((angle > 0.0f) && (sign < 0))
				{
					racer->steer(seconds, -min(angle / 1.11f, 1.0f));
				}
				else
				{
					racer->steer(seconds, 0.0f);
				}

				/****************************************************/

				racer->accelerate(seconds, intention.acceleration + speedBoost->getBoostValue());

				racer->applyForces(seconds);

				break;
			}
		case COMPUTER:
			{
				/*
					The computer travels at a base speed unless affected by particular modifiers.
					These modifiers can include things like speed boost, and waypoint types.
					Each waypoint can be a different type, if it is a standard waypoint, the vehicle will not
					change speed.  If it is a turn point or sharp point though, it will travel at slower speeds
					to make turns more successfully.
				*/
				float baseSpeed = 0.8f;
				hkVector4 vel = racer->body->getLinearVelocity();
				float velocity = vel.dot3(racer->drawable->getZhkVector());
				if(currentWaypoint+1 != 80){
					if(waypoints[currentWaypoint+1]->getWaypointType() == TURN_POINT && velocity > 70.0f){
						baseSpeed = 0.0f;
					}
					if(waypoints[currentWaypoint+1]->getWaypointType() == SHARP_POINT && velocity > 45.0f){
						baseSpeed = 0.0f;
					}
				}

				/* 
					Currently, the AI only uses a speedboost if it is at waypoint 35 (the middle of the
					ramp on the racetrack), and is traveling too slow to make the jump.
				*/
				if(!speedBoost->onCooldown() && currentWaypoint == 35 && velocity < 40){
					speedBoost->startCooldownTimer();
				}

				if(speedBoost->onCooldown()){
					char buf1[33];
					speedBoost->updateCooldown(seconds);
					_itoa_s(speedBoost->getCooldownTime(), buf1, 10);
				}

				racer->accelerate(seconds, baseSpeed + speedBoost->getBoostValue());

				/************* STEERING CALCULATIONS *************/

				bool targetAssigned = false;
				bool avoidanceEngaged = false;
				Racer* target;
				for(int i = 0; i < 5; i++){ // Determines if any racers are within an acceptable range to go into attack mode
					if(racers[i]->getIndex() != racer->getIndex()){ // If it is a racer other than itself
						hkVector4 position = racers[i]->body->getPosition();
						float angle = calculateAngleToPosition(&position);
						hkVector4 A = racer->drawable->getZhkVector();
						hkVector4 B;
						B.setXYZ(racer->lookDir);
						B(1) = 0.0f;

						B.dot3(A);
						// Sign determines if it is pointing to the right or the left of the current waypoint
						float sign = B.dot3(racer->drawable->getXhkVector());
						hkSimdReal distance = (racers[i]->body->getPosition()).distanceTo(racer->body->getPosition());
						hkVector4 vel = racers[i]->body->getLinearVelocity();
						float velocity = vel.dot3(racers[i]->drawable->getZhkVector());
						if(distance.isLess(60) && angle < 0.34906 && velocity > 30){ // add a speed condition here (speed > 40)
							targetAssigned = true; // If there is a target, attack mode (targetAssigned) is enabled, and the target determined
							target = racers[i];
							break;
						}
						else if(distance.isLess(40) && angle < 0.34906 && velocity <= 30){
							avoidanceEngaged = true;
						}
					}
				}
				if(targetAssigned){ // Once targeted, trys to aim at the racer, and when aiming close enough, shoots the laser
					hkVector4 targetPos = target->body->getPosition();
					hkVector4 shooterPos = racer->body->getPosition();

					shooterPos(1) += 2.0f;
					targetPos.sub(shooterPos);
					targetPos.normalize3();

					racer->lookDir.setXYZ(targetPos);
					

					if (!rocket->onCooldown())
					{
						rocket->startCooldownTimer();
						racer->fireRocket();
						targetAssigned = false;
					}

					if(rocket->onCooldown()){
						rocket->updateCooldown(seconds);
					}
				}
				else if(avoidanceEngaged){
					racer->steer(seconds, 1.0f);
				}
				else{
					/* Using the indexer in place of currentWaypoint would allow the ai to look one waypoint ahead for steering.
					---- For the current map, this is a bad design.
					int indexer;
					if(currentWaypoint == 79){
						indexer = 0;
					}
					else{
						indexer = currentWaypoint+1;
					}
					*/
					hkVector4 position = waypoints[currentWaypoint]->wpPosition;

					float angle = calculateAngleToPosition(&position);

					hkVector4 A = racer->drawable->getZhkVector();
					hkVector4 B;
					B.setXYZ(racer->lookDir);
					B(1) = 0.0f;

					B.dot3(A);
					// Sign determines if it is pointing to the right or the left of the current waypoint
					float sign = B.dot3(racer->drawable->getXhkVector());

			

					// The computer only turns if it is pointing away from the current waypoint by more than 20
					// degrees in either direction.
					if ((angle > 0.0f) && (sign > 0))
					{
						racer->steer(seconds, min(angle / 1.11f, 1.0f));
					}
					else if ((angle > 0.0f) && (sign < 0))
					{
						racer->steer(seconds, -min(angle / 1.11f, 1.0f));
					}
					else
					{
						racer->steer(seconds, 0.0f);
					}
				}

				
				/****************************************************/

				racer->applyForces(seconds);

				racer->computeRPM();
				
				break;
			}

	}
	/*
		If the computer is stuck, whether flipped, on its side,
		driving into a wall, or unable to move, if its position does
		not change for a particular amount of time, it will reset its location
		to its current waypoints location.
	*/
	
	hkVector4 currentPosition = racer->body->getPosition();
	int distanceTo = (int)currentPosition.distanceTo(lastPosition);
	newTime = time(NULL);
	int timeDiff = (int)difftime(newTime, oldTime);
	if(timeDiff > 3){
		if(distanceTo < 1){
			D3DXVECTOR3 cwPosition = waypoints[currentWaypoint]->drawable->getPosition();
			D3DXVECTOR3 nextPosition = waypoints[currentWaypoint+1]->drawable->getPosition();
			hkVector4 wayptVec;
			wayptVec.set(nextPosition.x, nextPosition.y, nextPosition.z);

			wayptVec.sub(hkVector4(cwPosition.x, cwPosition.y, cwPosition.z));

			hkVector4 resetPosition;
			resetPosition.set(cwPosition.x, cwPosition.y, cwPosition.z);

			
			racer->reset(&resetPosition, 0);
			wayptVec(1) = 0.0f;
			wayptVec.normalize3();
 
			hkVector4 z = racer->drawable->getZhkVector();
			hkVector4 x = racer->drawable->getXhkVector();
 
			float angle = hkMath::acos(z.dot3(wayptVec)); // angle is between 0 and 180
			
			// if the vector is on the LEFT side of the car...
			if (x.dot3(wayptVec) < 0.0f)
				angle *= -1.0f;
 
			rotationAngle = angle;
 
			racer->reset(&resetPosition, angle);
			calculateAngleToPosition(&(hkVector4(nextPosition.x, nextPosition.y, nextPosition.z)));
		}
		else{
			lastPosition = currentPosition;
		}
		oldTime = newTime;
	}

	overallPosition = currentWaypoint + (currentLap-1)*80; // 80 represents the number of waypoints
}

float AIMind::calculateAngleToPosition(hkVector4* position)
{
	racer->lookDir = *position;
	racer->lookDir.sub3clobberW(racer->body->getPosition());
	racer->lookDir.normalize3();

	hkVector4 A = racer->drawable->getZhkVector();
	hkVector4 C = racer->body->getPosition();
	hkVector4 B;
	B.setXYZ(racer->lookDir);
	B(1) = 0.0f;

	float angle = acos(B.dot3(A));

	return angle;
}

// Returns the current time left on the checkpoint timer
int AIMind::getCheckpointTime()
{
	return checkPointTime;
}

// When an AI reaches its waypoint, it will update its goal to the next waypoint
void AIMind::updateWaypointsAndLap(float seconds, Waypoint* waypoints[])
{
	int prevWaypoint;
	if(currentWaypoint - 1 == -1){
		prevWaypoint = 79;
	}
	else{
		prevWaypoint = currentWaypoint - 1;
	}
	D3DXVECTOR3 current = waypoints[currentWaypoint]->drawable->getPosition();
	hkVector4* currentPos = new hkVector4(current.x, current.y, current.z);
	D3DXVECTOR3 prev = waypoints[prevWaypoint]->drawable->getPosition();
	hkVector4* prevPos = new hkVector4(prev.x, prev.y, prev.z);

	hkSimdReal distanceOfRacer = waypoints[currentWaypoint]->wpPosition.distanceTo(getRacerPosition());

	if(waypoints[currentWaypoint]->passedWaypoint(currentPos, prevPos, &racer->body->getPosition())){
		if(waypoints[currentWaypoint]->getWaypointType() == LAP_POINT){
			currentLap += 1;
		}
	}
	if(waypoints[currentWaypoint]->passedWaypoint(currentPos, prevPos, &racer->body->getPosition())
		|| (distanceOfRacer.isLess(30) && waypoints[currentWaypoint]->getWaypointType() != LAP_POINT)){
		if(currentWaypoint == 79){
			currentWaypoint = 0;
		}
		else{
			currentWaypoint += 1;
		}
	}
}

// Switches between whether the racer is being controlled by a player or computer
void AIMind::togglePlayerComputerAI()
{
	if(racerType == COMPUTER){
		racerType = PLAYER;
	}
	else{
		racerType = COMPUTER;
	}
}

/*
	When a racer gets a kill, this function will determine 
	an ability to upgrade and change its associated paramaters.
*/
void AIMind::upgrade()
{
	int laserLevel = laser->getAbilityLevel();
	int speedLevel = speedBoost->getAbilityLevel();
	bool upgradeLaser = false;
	bool upgradeSpeed = false;

	if(laserLevel == speedLevel){
		srand((unsigned)time(0));
		int random_integer = rand()%100;
		if(random_integer > 50){
			upgradeLaser = true;
		}
		else if(random_integer > 0){
			upgradeSpeed = true;
		}
	}
	else{
		if(laserLevel < speedLevel){
			upgradeLaser = true;
		}
		else if(speedLevel < laserLevel){
			upgradeSpeed = true;
		}
	}

	if(upgradeLaser){
		if(laserLevel < 3){
			laser->update(laserLevel + 1);

			// The following method in racer was removed, since we're changing
			// the game's mechanics
			//racer->setDamageOutput(laser->getLaserDamage());
		}
	}
	else if(upgradeSpeed){
		if(speedLevel < 3){
			speedBoost->update(speedLevel + 1);
		}
	}
}

void AIMind::downgrade()
{
	int laserLevel = laser->getAbilityLevel();
	int speedLevel = speedBoost->getAbilityLevel();
	bool downgradeLaser = false;
	bool downgradeSpeed = false;

	if(laserLevel == speedLevel){
		srand((unsigned)time(0));
		int random_integer = rand()%100;
		if(random_integer > 50){
			downgradeLaser = true;
		}
		else if(random_integer > 0){
			downgradeSpeed = true;
		}
	}
	else{
		if(laserLevel > speedLevel){
			downgradeLaser = true;
		}
		else if(speedLevel > laserLevel){
			downgradeSpeed = true;
		}
	}

	if(downgradeLaser){
		if(laserLevel > 1){
			laser->update(laserLevel - 1);

			// The following method has been removed since we're
			// changing game mechanics
			//racer->setDamageOutput(laser->getLaserDamage());
		}
	}
	else if(downgradeSpeed){
		if(speedLevel > 1){
			speedBoost->update(speedLevel - 1);
		}
	}
}

// Returns the current lap that the racer is on
int AIMind::getCurrentLap()
{
	return currentLap;
}

// Returns the ID of the current waypoint the racer is trying to reach
int AIMind::getCurrentWaypoint()
{
	return currentWaypoint;
}

// Returns how much time is left on the cooldown of a racer
int AIMind::getSpeedCooldown()
{
	return speedBoost->getCooldownTime();
}

// A value based on how many laps and waypoints a racer has reached
int AIMind::getOverallPosition()
{
	return overallPosition;
}

// Sets the number representation of what place a racer is in (like 1st place, 2nd place, etc)
void AIMind::setPlacement(int place)
{
	placement = place;
}

hkVector4 AIMind::getRacerPosition()
{
	return racer->body->getPosition(); 
}

// Returns the number representation of what place a racer is in (like 1st place, 2nd place, etc)
int AIMind::getPlacement()
{
	return placement;
}

int AIMind::getLaserLevel()
{
	return laser->getAbilityLevel();
}

int AIMind::getSpeedLevel()
{
	return speedBoost->getAbilityLevel();
}

int AIMind::getCurrentCheckpoint()
{
	return checkPointTimer->getCurrentCheckpoint();
}

float AIMind::getRotationAngle()
{
	return rotationAngle;
}
