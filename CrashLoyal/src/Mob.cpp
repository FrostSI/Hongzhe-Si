#include "Mob.h"

#include <memory>
#include <limits>
#include <stdlib.h>
#include <stdio.h>
#include "Building.h"
#include "Waypoint.h"
#include "GameState.h"
#include "Point.h"

int Mob::previousUUID;

Mob::Mob() 
	: pos(-10000.f,-10000.f)
	, nextWaypoint(NULL)
	, targetPosition(new Point)
	, state(MobState::Moving)
	, uuid(Mob::previousUUID)
	, attackingNorth(true)
	, health(-1)
	, targetLocked(false)
	, target(NULL)
	, lastAttackTime(0)
{
}

void Mob::Init(const Point& pos, bool attackingNorth)
{
	this->previousUUID = this->previousUUID + 1;
	this->uuid = this->previousUUID;

	health = GetMaxHealth();
	this->pos = pos;
	this->attackingNorth = attackingNorth;
	findClosestWaypoint();
}

std::shared_ptr<Point> Mob::getPosition() {
	return std::make_shared<Point>(this->pos);
}

bool Mob::findClosestWaypoint() {
	std::shared_ptr<Waypoint> closestWP = GameState::waypoints[0];
	float smallestDist = std::numeric_limits<float>::max();

	for (std::shared_ptr<Waypoint> wp : GameState::waypoints) {
		//std::shared_ptr<Waypoint> wp = GameState::waypoints[i];
		// Filter out any waypoints that are "behind" us (behind is relative to attack dir
		// Remember y=0 is in the top left
		if (attackingNorth && wp->pos.y > this->pos.y) {
			continue;
		}
		else if ((!attackingNorth) && wp->pos.y < this->pos.y) {
			continue;
		}

		float dist = this->pos.dist(wp->pos);
		if (dist < smallestDist) {
			smallestDist = dist;
			closestWP = wp;
		}
	}
	std::shared_ptr<Point> newTarget = std::shared_ptr<Point>(new Point);
	this->targetPosition->x = closestWP->pos.x;
	this->targetPosition->y = closestWP->pos.y;
	this->nextWaypoint = closestWP;
	
	return true;
}

void Mob::moveTowards(std::shared_ptr<Point> moveTarget, double elapsedTime) {
	Point movementVector;
	movementVector.x = moveTarget->x - this->pos.x;
	movementVector.y = moveTarget->y - this->pos.y;
	movementVector.normalize();
	movementVector *= (float)this->GetSpeed();
	movementVector *= (float)elapsedTime;
pos += movementVector;
}


void Mob::findNewTarget() {
	// Find a new valid target to move towards and update this mob
	// to start pathing towards it

	if (!findAndSetAttackableMob()) { findClosestWaypoint(); }
}

// Have this mob start aiming towards the provided target
// TODO: impliment true pathfinding here
void Mob::updateMoveTarget(std::shared_ptr<Point> target) {
	this->targetPosition->x = target->x;
	this->targetPosition->y = target->y;
}

void Mob::updateMoveTarget(Point target) {
	this->targetPosition->x = target.x;
	this->targetPosition->y = target.y;
}


// Movement related
//////////////////////////////////
// Combat related

int Mob::attack(int dmg) {
	this->health -= dmg;
	return health;
}

bool Mob::findAndSetAttackableMob() {
	// Find an attackable target that's in the same quardrant as this Mob
	// If a target is found, this function returns true
	// If a target is found then this Mob is updated to start attacking it
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		if (otherMob->attackingNorth == this->attackingNorth) { continue; }

		bool imLeft = this->pos.x < (SCREEN_WIDTH / 2);
		bool otherLeft = otherMob->pos.x < (SCREEN_WIDTH / 2);

		bool imTop = this->pos.y < (SCREEN_HEIGHT / 2);
		bool otherTop = otherMob->pos.y < (SCREEN_HEIGHT / 2);
		if ((imLeft == otherLeft) && (imTop == otherTop)) {
			// If we're in the same quardrant as the otherMob
			// Mark it as the new target
			this->setAttackTarget(otherMob);
			return true;
		}
	}
	return false;
}

// TODO Move this somewhere better like a utility class
int randomNumber(int minValue, int maxValue) {
	// Returns a random number between [min, max). Min is inclusive, max is not.
	return (rand() % maxValue) + minValue;
}

void Mob::setAttackTarget(std::shared_ptr<Attackable> newTarget) {
	this->state = MobState::Attacking;
	target = newTarget;
}

bool Mob::targetInRange() {
	float range = this->GetSize(); // TODO: change this for ranged units
	float totalSize = range + target->GetSize();
	return this->pos.insideOf(*(target->getPosition()), totalSize * 2);
}
// Combat related
////////////////////////////////////////////////////////////
// Collisions

// PROJECT 3: 
//  1) return a vector of mobs that we're colliding with
//  2) handle collision with towers & river 
std::shared_ptr<Mob> Mob::checkCollision() {
	float radius = 2;
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		// don't collide with yourself
		if (this->sameMob(otherMob)) { continue; }
		// PROJECT 3: YOUR CODE CHECKING FOR A COLLISION GOES HERE
		if (otherMob->pos.insideOf(this->pos, ((this->GetSize() + otherMob->GetSize()) * radius))) {
			//std::cout << this->uuid << ": " << "Collide" << std::endl;
			return std::shared_ptr<Mob>(otherMob);
		}
	}
	return std::shared_ptr<Mob>(nullptr);
}

void Mob::processCollision(std::shared_ptr<Mob> otherMob, double elapsedTime) {
	float ratio = 1;
	Point vDirection;
	vDirection.x = this->pos.x - otherMob->pos.x;
	vDirection.y = this->pos.y - otherMob->pos.y;
	vDirection.normalize();
	vDirection = vDirection * ratio * this->GetSpeed() * elapsedTime;
	this->pos += vDirection;
	// PROJECT 3: YOUR COLLISION HANDLING CODE GOES HERE
}

void Mob::checkAndProcessCollisionBuilding(double elapsedTime) {
	float radius = 2;
	float ratio = 2;
	Point vDirection;

	for (std::shared_ptr<Building> b : GameState::buildings) {
		if (b->getPoint().insideOf(this->pos, (this->GetSize()) * radius)) {
			vDirection.x = this->pos.x - b->getPoint().x;
			vDirection.y = this->pos.y - b->getPoint().y;
			break;
		}
	}

	if (RIVER_TOP_Y < this->pos.y + this->GetSize() && this->pos.y - this->GetSize() < RIVER_BOT_Y) {
		Point l, r;
		l.x = LEFT_BRIDGE_CENTER_X; l.y = LEFT_BRIDGE_CENTER_Y;
		r.x = RIGHT_BRIDGE_CENTER_X; r.y = RIGHT_BRIDGE_CENTER_Y;
		if(!(this->pos.insideOf(l, BRIDGE_HEIGHT - this->GetSize()) 
			|| this->pos.insideOf(r, BRIDGE_HEIGHT - this->GetSize()))){

			if (this->pos.y >= (RIVER_TOP_Y + RIVER_BOT_Y) / 2)
				vDirection.y = 2;
			if (this->pos.y < (RIVER_TOP_Y + RIVER_BOT_Y) / 2)
				vDirection.y = -2;

			if (this->pos.dist(l) <= this->pos.dist(r))
				vDirection.x = -1;
			else
				vDirection.x = 1;
		}
	}

	vDirection.normalize();
	vDirection = vDirection * ratio * this->GetSpeed() * elapsedTime;
	this->pos += vDirection;
}

// Collisions
///////////////////////////////////////////////
// Procedures

void Mob::attackProcedure(double elapsedTime) {
	if (this->target == nullptr || this->target->isDead()) {
		this->targetLocked = false;
		this->target = nullptr;
		this->state = MobState::Moving;
		return;
	}

	if (targetInRange()) {
		if (this->lastAttackTime >= this->GetAttackTime()) {
			// If our last attack was longer ago than our cooldown
			this->target->attack(this->GetDamage());
			this->lastAttackTime = 0; // lastAttackTime is incremented in the main update function
			return;
		}
	}
	else {
		std::shared_ptr<Mob> otherMob = this->checkCollision();
		if (otherMob) {
			this->processCollision(otherMob, elapsedTime);
		}
		this->checkAndProcessCollisionBuilding(elapsedTime);
		// If the target is not in range
		moveTowards(target->getPosition(), elapsedTime);
	}
}

void Mob::moveProcedure(double elapsedTime) {
	if (targetPosition) {
		moveTowards(targetPosition, elapsedTime);

		// Check for collisions
		if (this->nextWaypoint->pos.insideOf(this->pos, (this->GetSize() + WAYPOINT_SIZE))) {
			std::shared_ptr<Waypoint> trueNextWP = this->attackingNorth ?
												   this->nextWaypoint->upNeighbor :
												   this->nextWaypoint->downNeighbor;
			setNewWaypoint(trueNextWP);
		}

		// PROJECT 3: You should not change this code very much, but this is where your 
		// collision code will be called from
		std::shared_ptr<Mob> otherMob = this->checkCollision();
		if (otherMob) {
			this->processCollision(otherMob, elapsedTime);
		}
		this->checkAndProcessCollisionBuilding(elapsedTime);

		// Fighting otherMob takes priority always
		findAndSetAttackableMob();

	} else {
		// if targetPosition is nullptr
		findNewTarget();
	}
}

void Mob::update(double elapsedTime) {

	switch (this->state) {
	case MobState::Attacking:
		this->attackProcedure(elapsedTime);
		break;
	case MobState::Moving:
	default:
		this->moveProcedure(elapsedTime);
		break;
	}

	this->lastAttackTime += (float)elapsedTime;
}
