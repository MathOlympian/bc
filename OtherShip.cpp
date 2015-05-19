/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

//Extends from the general 'Ship' class
#include "IniFile.hpp"
#include "Angles.hpp"
#include "RadarData.hpp"
#include "Constants.hpp"
#include "OtherShip.hpp"

#include <iostream>

using namespace irr;

OtherShip::OtherShip (const std::string& name,const irr::core::vector3df& location, std::vector<Leg> legsLoaded, irr::scene::ISceneManager* smgr)
{

    this->name = name;
    //Load from individual boat.ini file
    std::string iniFilename = "Models/Othership/";
    iniFilename.append(name);
    iniFilename.append("/boat.ini");

    //load information about this model from its ini file
    std::string shipFileName = IniFile::iniFileToString(iniFilename,"FileName");

    //get scale factor from ini file (or zero if not set - assume 1)
    f32 scaleFactor = IniFile::iniFileTof32(iniFilename,"Scalefactor");
    if (scaleFactor==0.0) {
        scaleFactor = 1.0; //Default if not set
    }

    f32 yCorrection = IniFile::iniFileTof32(iniFilename,"YCorrection");

    std::string shipFullPath = "Models/Othership/"; //FIXME: Use proper path handling
    shipFullPath.append(name);
    shipFullPath.append("/");
    shipFullPath.append(shipFileName);

    //load mesh
    scene::IMesh* shipMesh = smgr->getMesh(shipFullPath.c_str());

    //scale and translate
    core::matrix4 transformMatrix;
    transformMatrix.setScale(core::vector3df(scaleFactor,scaleFactor,scaleFactor));
    transformMatrix.setTranslation(core::vector3df(0,yCorrection*scaleFactor,0));
    smgr->getMeshManipulator()->transform(shipMesh,transformMatrix);

    //add to scene node
	if (shipMesh==0) {
        //Failed to load mesh - load with dummy and continue - ToDo: should also flag this up to user
        ship = smgr->addCubeSceneNode(0.1);
    } else {
        ship = smgr->addMeshSceneNode( shipMesh, 0, -1);
    }

	ship->setMaterialFlag(video::EMF_NORMALIZE_NORMALS, true); //Normalise normals on scaled meshes, for correct lighting

    //store length and RCS information for radar etc
    length = ship->getBoundingBox().getExtent().Z;
    height = ship->getBoundingBox().getExtent().Y * 0.75; //Assume 3/4 of the mesh is above water
    rcs = 0.005*std::pow(length,3); //Default RCS, base radar cross section on length^3 (following RCS table Ship_RCS_table.pdf)

    //store initial x,y,z positions
    xPos = location.X;
    yPos = location.Y;
    zPos = location.Z;
    //speed and heading will come from leg data

    //Set lighting to use diffuse and ambient, so lighting of untextured models works
	if(ship->getMaterialCount()>0) {
        for(u32 mat=0;mat<ship->getMaterialCount();mat++) {
            ship->getMaterial(mat).ColorMaterial = video::ECM_DIFFUSE_AND_AMBIENT;
        }
    }

    //get light locations:
    u32 numberOfLights = IniFile::iniFileTou32(iniFilename,"NumberOfLights");
    if (numberOfLights>0) {
        for (u32 currentLight=1; currentLight<=numberOfLights; currentLight++) {
            f32 lightX = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightX",currentLight));
            f32 lightY = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightY",currentLight));
            f32 lightZ = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightZ",currentLight));

            u32 lightR = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightRed",currentLight));
            u32 lightG = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightGreen",currentLight));
            u32 lightB = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightBlue",currentLight));

            f32 lightStartAngle = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightStartAngle",currentLight)); //Degrees 0-360
            f32 lightEndAngle = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightEndAngle",currentLight)); //Degrees 0-720, should be greater than LightStartAngle
            f32 lightRange = IniFile::iniFileTof32(iniFilename,IniFile::enumerate1("LightRange",currentLight)); //Range (Nm)
            lightRange = lightRange * M_IN_NM; //Convert to metres

            //correct to local scaled coordinates
            lightX *= scaleFactor;
            lightY = (lightY+yCorrection)*scaleFactor;
            lightZ *= scaleFactor;

            //add this Nav light into array
            navLights.push_back(NavLight (ship,smgr,core::dimension2d<f32>(5, 5), core::vector3df(lightX,lightY,lightZ),video::SColor(255,lightR,lightG,lightB),lightStartAngle,lightEndAngle,lightRange));
        }
    }

    //store leg information
    legs=legsLoaded;
}

void OtherShip::update(irr::f32 deltaTime, irr::f32 scenarioTime, irr::f32 tideHeight, irr::core::vector3df viewPosition, irr::u32 lightLevel)
{

    //move according to leg information
    if (legs.empty()) {
        spd = 0;
        hdg = 0;
    } else {
        //Work out which leg we're on
        std::vector<Leg>::size_type currentLeg = findCurrentLeg(scenarioTime);

        spd = legs[currentLeg].speed*KTS_TO_MPS;
        hdg = legs[currentLeg].bearing;
    }

    xPos = xPos + sin(hdg*core::DEGTORAD)*spd*deltaTime;
    zPos = zPos + cos(hdg*core::DEGTORAD)*spd*deltaTime;
    yPos = tideHeight;

    //Set position & speed by calling ship methods
    setPosition(core::vector3df(xPos,yPos,zPos));
    setRotation(core::vector3df(0, hdg, 0)); //Global vectors

    //for each light, find range and angle
    for(std::vector<NavLight>::size_type currentLight = 0; currentLight<navLights.size(); currentLight++) {
        navLights[currentLight].update(scenarioTime,viewPosition, lightLevel);
    }

}

irr::f32 OtherShip::getLength() const
{
    return length;
}

irr::f32 OtherShip::getHeight() const
{
    return height;
}

irr::f32 OtherShip::getRCS() const
{
    return rcs;
}

std::string OtherShip::getName() const
{
    return name;
}

std::vector<Leg> OtherShip::getLegs() const
{
    return legs;
}

void OtherShip::changeLeg(int legNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime)
{

    //Check if leg exists, then if we are allowed to change this leg (current or future leg), and not the final 'stop' leg (hence legs.size()-1)
    if (legNumber >=0 && legNumber < legs.size() - 1 && legNumber >= findCurrentLeg(scenarioTime)) {

        //Store old information temporarily
        irr::f32 oldSpeed = legs.at(legNumber).speed;

        //Recalculate subsequent start times, only changing from the current point.
        //We can guarantee that there is a next leg, as we checked (legNumber < legs.size() - 1)

        irr::f32 newTimeRemaining;
        if ( legNumber == findCurrentLeg(scenarioTime) ) {
            //On current leg - calculate from current point only
            irr::f32 oldTimeRemaining = legs.at(legNumber+1).startTime - scenarioTime;
            if (distance < 0) {distance = fabs(oldSpeed)*oldTimeRemaining/SECONDS_IN_HOUR;} //If leg length is negative, ensure overall leg length doesn't change
            newTimeRemaining = SECONDS_IN_HOUR * distance / fabs(speed); //The adjusted leg distance starts from now
            legs.at(legNumber).startTime = scenarioTime; // New leg effectively starts now
        } else {
            //On subsequent leg - calculate for whole leg
            irr::f32 oldTimeRemaining = legs.at(legNumber+1).startTime - legs.at(legNumber).startTime;
            if (distance < 0) {distance = fabs(oldSpeed)*oldTimeRemaining/SECONDS_IN_HOUR;} //If leg length is negative, ensure overall leg length doesn't change
            newTimeRemaining = SECONDS_IN_HOUR * distance / fabs(speed);
            //No need to change start time.
        }

        //Change this leg
        legs.at(legNumber).bearing = bearing;
        legs.at(legNumber).speed = speed;
        legs.at(legNumber).distance = distance; //Store for later reference

        //Set start time of the next leg (guaranteed to exist)
        legs.at(legNumber + 1).startTime = legs.at(legNumber).startTime + newTimeRemaining;
        //For the remaining legs (which may not exist)
        for (int i = legNumber + 2; i < legs.size(); i++) {
            legs.at(i).startTime = legs.at(i-1).startTime + SECONDS_IN_HOUR*legs.at(i-1).distance/legs.at(i-1).speed;
        }

    } //Check leg exists & can be changed

}

void OtherShip::addLeg(int afterLegNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime)
{

    //Check if leg exists, and is before the 'stop leg'
    if (afterLegNumber >=0 && afterLegNumber < (legs.size() - 1)) {

        //if we're on the stop leg
        if (findCurrentLeg(scenarioTime) == (legs.size()-1)) {

            //If the 'after' leg is the penultimate, add a leg before the stop one, starting now
            if (afterLegNumber == (legs.size()-2))  {
                Leg newLeg;
                newLeg.bearing = bearing;
                newLeg.speed = speed;
                newLeg.distance = distance;
                newLeg.startTime = scenarioTime;

                legs.insert(legs.end(), newLeg); //Insert before final leg
            }
        //else check that the 'after' leg is current or future
        } else if (afterLegNumber >= findCurrentLeg(scenarioTime)) {
            Leg newLeg;
            newLeg.bearing = bearing;
            newLeg.speed = speed;
            newLeg.distance = distance;
            newLeg.startTime = legs.at(afterLegNumber + 1).startTime; //This leg starts when the next leg would have started

            legs.insert(legs.begin()+afterLegNumber+1, newLeg); //Insert leg
        }

        //set start time of subsequent legs
        //For the remaining legs (which may not exist)
        for (int i = afterLegNumber + 2; i < legs.size(); i++) {
            legs.at(i).startTime = legs.at(i-1).startTime + SECONDS_IN_HOUR*legs.at(i-1).distance/legs.at(i-1).speed;
        }


    } //Check leg exists & can be changed

}

void OtherShip::deleteLeg(int legNumber, irr::f32 scenarioTime)
{

    //Check if leg exists, then if we are allowed to change this leg (current or future leg), and not the final 'stop' leg (hence legs.size()-1)
    if (legNumber >=0 && legNumber < legs.size() - 1 && legNumber >= findCurrentLeg(scenarioTime)) {

        //We can guarantee that there is a next leg, as we checked (legNumber < legs.size() - 1)

        //Current or future leg?
        if (legNumber == findCurrentLeg(scenarioTime)) {
            //Current leg
            //Set next leg start time to now: Set start time of the next leg (guaranteed to exist)
            legs.at(legNumber + 1).startTime = scenarioTime;

        } else {
            //Future leg
            //Set next leg start time to the start time of the leg we're removing
            legs.at(legNumber + 1).startTime = legs.at(legNumber).startTime;
        }

        //adjust start time of subsequent legs
        //For the remaining legs (which may not exist)
        for (int i = legNumber + 2; i < legs.size(); i++) {
            legs.at(i).startTime = legs.at(i-1).startTime + SECONDS_IN_HOUR*legs.at(i-1).distance/legs.at(i-1).speed;
        }

        //Remove this leg
        legs.erase(legs.begin() + legNumber);

    } //Check leg exists & can be changed

}

RadarData OtherShip::getRadarData(irr::core::vector3df scannerPosition) const
//Get data for OtherShip (number) relative to scannerPosition
//Similar code in Buoy.cpp
{
    RadarData radarData;

    irr::core::vector3df contactPosition = getPosition();
    irr::core::vector3df relativePosition = contactPosition-scannerPosition;

    radarData.relX = relativePosition.X;
    radarData.relZ = relativePosition.Z;
    radarData.angle = relativePosition.getHorizontalAngle().Y;
    radarData.range = relativePosition.getLength();
    radarData.heading = getHeading();

    radarData.height=getHeight();
    radarData.solidHeight=0.5*radarData.height; //Fixme: Allow this to be set as a parameter.
    //radarData.radarHorizon=99999; //ToDo: Implement when ARPA is implemented
    radarData.length=getLength();
    radarData.rcs=getRCS();

    //Calculate angles and ranges to each end of the contact
    irr::f32 relAngle1 = Angles::normaliseAngle(irr::core::RADTODEG*std::atan2( radarData.relX + 0.5*radarData.length*std::sin(irr::core::DEGTORAD*radarData.heading), radarData.relZ + 0.5*radarData.length*std::cos(irr::core::DEGTORAD*radarData.heading) ));
    irr::f32 relAngle2 = Angles::normaliseAngle(irr::core::RADTODEG*std::atan2( radarData.relX - 0.5*radarData.length*std::sin(irr::core::DEGTORAD*radarData.heading), radarData.relZ - 0.5*radarData.length*std::cos(irr::core::DEGTORAD*radarData.heading) ));
    irr::f32 range1 = std::sqrt(std::pow(radarData.relX + 0.5*radarData.length*std::sin(irr::core::DEGTORAD*radarData.heading),2) + std::pow(radarData.relZ + 0.5*radarData.length*std::cos(irr::core::DEGTORAD*radarData.heading),2));
    irr::f32 range2 = std::sqrt(std::pow(radarData.relX - 0.5*radarData.length*std::sin(irr::core::DEGTORAD*radarData.heading),2) + std::pow(radarData.relZ - 0.5*radarData.length*std::cos(irr::core::DEGTORAD*radarData.heading),2));
    radarData.minRange=std::min(range1,range2);
    radarData.maxRange=std::max(range1,range2);
    radarData.minAngle=std::min(relAngle1,relAngle2);
    radarData.maxAngle=std::max(relAngle1,relAngle2);

    //Initial defaults: Will need changing with full implementation
    radarData.hidden=false;
    radarData.racon=""; //Racon code if set
    radarData.raconOffsetTime=0.0;
    radarData.SART=false;

    return radarData;
}

std::vector<Leg>::size_type OtherShip::findCurrentLeg(irr::f32 scenarioTime)
{
    std::vector<Leg>::size_type currentLeg;

    for(currentLeg = 0; currentLeg<legs.size()-1; currentLeg++) {
        if (legs[currentLeg].startTime <=scenarioTime && legs[currentLeg+1].startTime > scenarioTime ) {
            break;
        }
    }
    //currentLeg is now the correct leg, or the last leg, which is a 'stopped' leg. (true as we run currentLeg++ once after the check (currentLeg<legs.size()-1) if the 'break' isn't reached

    return currentLeg;
}
