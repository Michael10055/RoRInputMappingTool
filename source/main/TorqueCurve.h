/*
This source file is part of Rigs of Rods
Copyright 2005,2006,2007,2008,2009 Pierre-Michel Ricordel
Copyright 2007,2008,2009 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __TorqueCurve_H__
#define __TorqueCurve_H__

#include "OgrePrerequisites.h"
#include "rormemory.h"

/**
 * @file TorqueCurve.h
 * @version 1
 * @brief torquecurve loader.
 * @authors flypiper
 * @authors Thomas Fischer (thomas{AT}rigsofrods{DOT}com)
 */

/**
 *  @brief This class loads and processes a torque curve for a truck.
 */
class TorqueCurve : public MemoryAllocatedObject
{
public:
	const static Ogre::String customModel;

	TorqueCurve();  //!< Constructor
	~TorqueCurve(); //!< Destructor

	/**
	 * Returns the calculated engine torque based on the given rpmRatio, interpolating the torque curve spline.
	 * @param rpmRatio Ratio of current engine RPM to maximum RPM (value between 0 and 1).
	 * @return Calculated engine torque.
	 */
	Ogre::Real getEngineTorque(Ogre::Real rpmRatio);

	/**
	 * Sets the torque model which is used for the truck.
	 * @param name name of the torque model which should be used.
	 * @return 0 on success, 1 if torque model is not found.
	 */
	int setTorqueModel(Ogre::String name);

	/**
	 * Processes a line of a torque curve.
	 * @param line Line of the file.
	 * @param model Torque model name (i.e. 'turbodiesel').
	 * @return 0 on success, everything else on error
	 */
	int processLine(Ogre::String line, Ogre::String model = customModel);

	/**
	 * Returns the used spline.
	 * @return The torque spline used by the truck.
	 */
	Ogre::SimpleSpline *getUsedSpline() { return usedSpline; };

	/**
	 * Returns the name of the torque model used by the truck.
	 * @return The name of the torque model used by the truck.
	 */
	Ogre::String getTorqueModel() { return usedModel; };

protected:
	Ogre::SimpleSpline *usedSpline;                        //!< spline which is used for calculating the torque, set by setTorqueModel().
	Ogre::String usedModel;                                //!< name of the torque model used by the truck.
	std::map < Ogre::String, Ogre::SimpleSpline > splines; //!< container were all torque curve splines are stored in.

	/**
	 * Loads default torque models from the 'torque_models.cfg' file.
	 * @return 0 on success, 1 if 'torque_model.cfg' file not found.
	 */
	int loadDefaultTorqueModels();

	/**
	 * Processes the given vector.
	 * Adds points to a torque curve spline; or if a new model is found, generating a new spline.
	 * @param args Vector of arguments of the line which should be processed.
	 * @param model Torque model name (i.e. 'turbodiesel')
	 * @return setTorqueModel() called if one argument given, 1 on error, 0 on success
	 */
	int processLine(Ogre::vector< Ogre::String >::type args, Ogre::String model);
};


#endif
