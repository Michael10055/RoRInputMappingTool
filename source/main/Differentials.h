#ifndef __DIFFERENTIALS_H__
#define __DIFFERENTIALS_H__

#include "OgrePrerequisites.h"
#include <vector>

#define MAX_DIFFS 3
struct differential_data_t
{
	Ogre::Real speed[2];
	Ogre::Real delta_rotation; // sign is first relative to the second
	Ogre::Real out_torque[2];
	Ogre::Real in_torque;
	Ogre::Real dt;
};

enum DiffType
{
	SPLIT_DIFF = 0,
	VISCOUS_DIFF,
	TC_DIFF,
	OPEN_DIFF,
	LOCKED_DIFF
};

typedef void (*diff_callback)( differential_data_t& diff_data);

//! class created from a struct will be ugly for a while.
// ugly code is not merged. fixed that up -- thomas
class Axle
{
public:
    Axle();

	int wheel_1; //! array location of wheel 1
	int wheel_2; //! array location of wheel 2
	//! difference of rotational position between two axles... a kludge at best
	Ogre::Real delta_rotation;
	//! torsion spring rate binding wheels together.
	Ogre::Real torsion_rate;
	Ogre::Real torsion_damp;

	//! average wheel speed,
	Ogre::Real avg_speed;
	//! not used ATM will be for future use
	Ogre::Real gear_ratio;
	//! which group this axle belong to, not used ATM
	unsigned int axle_group;

	void addDiffType(DiffType diff);
	const std::vector<DiffType>& availableDiffs();
	void toggleDiff();
	void calcTorque( differential_data_t& diff_data );
	Ogre::String getDiffTypeName();

	//! a differential that always splits the torque evenly, this is the original method
    static void calcSeperatedDiff( differential_data_t& diff_data);
    //! viscous coupler, the greater the difference in speed the more torque is transfered
    static void calcViscousDiff( differential_data_t& diff_data );
    //! initial attempt at traction controll
    static void calcTCDiff( differential_data_t& diff_data );
    //! more power goes to the faster spining wheel
    static void calcOpenDiff( differential_data_t& diff_data );
    //! ensures both wheels rotate at the the same speed
    static void calcLockedDiff( differential_data_t& diff_data );
private:

    //! type of differential
    unsigned int which_diff;
    unsigned int free_diff;
    diff_callback current_callback;

    //! available diffs
    std::vector<DiffType> available_diff_method;

    static diff_callback getDiffEquation(DiffType type);
};


#endif // __DIFFERENTIALS_H__
