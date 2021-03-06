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
#include "turboprop.h"

#ifdef USE_ANGELSCRIPT
#include "ScriptEngine.h"
#endif

Turboprop::Turboprop(SceneManager *manager, char* propname, node_t *nd, int nr, int nb, int np1, int np2, int np3, int np4, int tqn, float power, char* propfoilname, int mnumber, int trucknum, bool disable_smoke, bool ispiston, float fpitch, bool _heathaze)
{
	failed=false;
	failedold=false;
	heathaze=_heathaze;
	number=mnumber;
	this->trucknum=trucknum;
#ifdef USE_OPENAL
	ssm=SoundScriptManager::getSingleton();
	switch (number)
	{
	case 0: mod_id=SS_MOD_AEROENGINE1;src_id=SS_TRIG_AEROENGINE1;thr_id=SS_MOD_THROTTLE1;break;
	case 1: mod_id=SS_MOD_AEROENGINE2;src_id=SS_TRIG_AEROENGINE2;thr_id=SS_MOD_THROTTLE2;break;
	case 2: mod_id=SS_MOD_AEROENGINE3;src_id=SS_TRIG_AEROENGINE3;thr_id=SS_MOD_THROTTLE3;break;
	case 3: mod_id=SS_MOD_AEROENGINE4;src_id=SS_TRIG_AEROENGINE4;thr_id=SS_MOD_THROTTLE4;break;
	case 4: mod_id=SS_MOD_AEROENGINE5;src_id=SS_TRIG_AEROENGINE5;thr_id=SS_MOD_THROTTLE5;break;
	case 5: mod_id=SS_MOD_AEROENGINE6;src_id=SS_TRIG_AEROENGINE6;thr_id=SS_MOD_THROTTLE6;break;
	case 6: mod_id=SS_MOD_AEROENGINE7;src_id=SS_TRIG_AEROENGINE7;thr_id=SS_MOD_THROTTLE7;break;
	case 7: mod_id=SS_MOD_AEROENGINE8;src_id=SS_TRIG_AEROENGINE8;thr_id=SS_MOD_THROTTLE8;break;
	default: mod_id=SS_MOD_NONE;src_id=SS_TRIG_NONE;thr_id=SS_MOD_NONE;
	}
#endif //OPENAL
	is_piston=ispiston;
	fixed_pitch=fpitch;
	torquenode=tqn;
	rotenergy=0;
	pitchspeed=5.0; //in degree/sec
	numblades=4;
	twistmap[0]=2;
	twistmap[1]=6;
	twistmap[2]=10;
	twistmap[3]=19;
	twistmap[4]=32;
	nodes=nd;
	noderef=nr;
	nodeback=nb;
	nodep[0]=np1;
	nodep[1]=np2;
	if (torquenode!=-1)
	{
		Plane pplane=Plane((nd[nr].RelPosition-nd[nb].RelPosition).normalisedCopy(), 0.0);
		Vector3 apos=pplane.projectVector(nd[nr].RelPosition);
		Vector3 tpos=pplane.projectVector(nd[tqn].RelPosition);
		torquedist=(apos-tpos).length();
	} else torquedist=1.0;
	if (np3==-1) numblades=2;
	else
	{
		nodep[2]=np3;
		if (np4==-1) numblades=3;
		else nodep[3]=np4;
	}
	propwash=0;
	free_vpale=0;
	vspinner=0;
	timer=0;
	lastflip=0;
	warmuptime=14.0;
	airfoil=new Airfoil(propfoilname);
	fullpower=power;
	max_torque=9549.3*fullpower/1000.0;
	radius=(nodes[noderef].RelPosition-nodes[nodep[0]].RelPosition).length();
	//bladewidth=radius/5.75;
	bladewidth=0.4;
	proparea=3.14159*radius*radius;
	airdensity=1.225;
//	regspeed=245.0*60.0/(2.0*3.14159*radius);//1010.0 //compute regulated speed for a tip speed of 245m/s
	regspeed=1010;
	maxrevpitch=-5.0;
	//smoke visual
	if (disable_smoke)
	{
		smokeNode=0;
		smokePS=0;
	}
	else
	{
		char dename[256];
		sprintf(dename,"%s-smoke", propname);
		smokeNode=manager->getRootSceneNode()->createChildSceneNode();
		smokePS=manager->createParticleSystem(dename, "tracks/TurbopropSmoke");
		if (smokePS) 
		{
			smokeNode->attachObject(smokePS);
			smokePS->setCastShadows(false);
		}

		heathazePS=0;
		if(heathaze)
		{
			sprintf(dename,"%s-smoke-heat", propname);
			heathazePS=manager->createParticleSystem(dename, "tracks/TurbopropHeatHaze");
			smokeNode->attachObject(heathazePS);
			heathazePS->setCastShadows(false);
		}
	}

	reset();
}

void Turboprop::updateVisuals()
{
	//visuals
	if (rpm>200)
	{
		int i;
		for (i=0; i<free_vpale; i++) vpales[i]->setVisible(false);
		vspinner->setVisible(true);
	}
	else
	{
		int i;
		for (i=0; i<free_vpale; i++) vpales[i]->setVisible(true);
		vspinner->setVisible(false);
	}
	//smoke
	if (smokeNode)
	{
		smokeNode->setPosition(nodes[nodeback].AbsPosition);
		ParticleEmitter *emit=smokePS->getEmitter(0);
		ParticleEmitter *hemit=0;
		if(heathazePS)
			hemit=heathazePS->getEmitter(0);
		Vector3 dir=nodes[nodeback].RelPosition-nodes[noderef].RelPosition;
		emit->setDirection(dir);
		emit->setParticleVelocity(propwash-propwash/10, propwash+propwash/10);
		if(hemit)
		{
			hemit->setDirection(dir);
			hemit->setParticleVelocity(propwash-propwash/10, propwash+propwash/10);
		}
		if (!failed)
		{
			if (ignition) 
			{
				emit->setEnabled(true);
				emit->setColour(ColourValue(0.0,0.0,0.0,0.03+throtle*0.05));
				emit->setTimeToLive((0.03+throtle*0.05)/0.1);
				if(hemit)
				{
					hemit->setEnabled(true);
					hemit->setTimeToLive((0.03+throtle*0.05)/0.1);
				}
			}
			else 
			{
				emit->setEnabled(false);
				if(hemit)
					hemit->setEnabled(false);
			}
		}
		else
		{
			emit->setDirection(Vector3(0,1,0));
			emit->setParticleVelocity(5, 9);
			emit->setEnabled(true);
			emit->setColour(ColourValue(0.0,0.0,0.0,0.1));
			emit->setTimeToLive(0.1/0.1);
			if(hemit)
			{
				hemit->setDirection(Vector3(0,1,0));
				hemit->setParticleVelocity(5, 9);
				hemit->setEnabled(true);
				hemit->setTimeToLive(0.1/0.1);
			}
		}
	}

#ifdef USE_ANGELSCRIPT
	if(failed != failedold)
	{
		ScriptEngine::getSingleton().triggerEvent(ScriptEngine::SE_TRUCK_ENGINE_FIRE, trucknum);
		failedold = failed;
	}
#endif
}

void Turboprop::updateForces(float dt, int doUpdate)
{
	if (doUpdate)
	{
		//tropospheric model valid up to 11.000m (33.000ft)
		float altitude=nodes[noderef].AbsPosition.y;
		float sea_level_temperature=273.15+15.0; //in Kelvin
		float sea_level_pressure=101325; //in Pa
		float airtemperature=sea_level_temperature-altitude*0.0065; //in Kelvin
		float airpressure=sea_level_pressure*pow(1.0-0.0065*altitude/288.15, 5.24947); //in Pa
		airdensity=airpressure*0.0000120896;//1.225 at sea level
#ifdef USE_OPENAL
		//sound update
		if(ssm) ssm->modulate(trucknum, mod_id, rpm);
#endif //OPENAL
	}

	timer+=dt;
	//evaluate the rotation speed
	float velacc=0;
	for (int i=0; i<numblades; i++) velacc+=(nodes[nodep[i]].Velocity-nodes[noderef].Velocity).length();
	rpm=(velacc/numblades)*9.549/radius;
	//check for broken prop
	Vector3 avg=Vector3::ZERO;
	for (int i=0; i<numblades; i++) avg+=nodes[nodep[i]].RelPosition;
	avg=avg/numblades;
	if ((avg-nodes[noderef].RelPosition).length()>0.4)
	{
		failed=true;
	}
	//evaluate engine power
	float enginepower=0; //in kilo-Watt
	float warmupfactor=1.0;
	if (warmup) 
	{
		warmupfactor=(timer-warmupstart)/warmuptime;
		if (warmupfactor>=1.0) warmup=false;
	}
	float revpenalty=1.0;
	if (reverse) revpenalty=0.5;
	if (!failed && ignition) enginepower=(0.0575+throtle*revpenalty*0.9425)*fullpower*warmupfactor;
	//the magic formula
	float enginecouple=0.0; //in N.m
	if (rpm>10.0) enginecouple=9549.3*enginepower/rpm;
	else enginecouple=9549.3*enginepower/10.0;
	indicated_torque=enginecouple;
	
	if (torquenode!=-1)
	{
		Vector3 along=nodes[noderef].RelPosition-nodes[nodeback].RelPosition;
		Plane ppl=Plane(along, 0);
		Vector3 orth=ppl.projectVector(nodes[noderef].RelPosition)-ppl.projectVector(nodes[torquenode].RelPosition);
		Vector3 cdir=orth.crossProduct(along);
		cdir.normalise();
		nodes[torquenode].Forces+=(enginecouple/torquedist)*cdir;
	}
	
	float tipforce=(enginecouple/radius)/numblades;
	//okay, now we know the contribution from the engine

	//pitch
	if (fixed_pitch>0) pitch=fixed_pitch; else
	{
		if (!reverse)
		{
			if (throtle<0.01)
			{
				//beta range
				if (pitch>0 && rpm<regspeed*1.4) pitch-=pitchspeed*dt;
				if (rpm>regspeed*1.4) pitch+=pitchspeed*dt;
			}
			else
			{
				float dpitch=rpm-regspeed;
				if (dpitch>pitchspeed) dpitch=pitchspeed;
				if (dpitch<-pitchspeed) dpitch=-pitchspeed;
				if (!(dpitch<0 && pitch<0) && !(dpitch>0 && pitch>45)) pitch+=dpitch*dt;
			}
		} else
		{
			if (rpm<regspeed*1.1)
			{
				if (pitch<-4.0) pitch+=pitchspeed*dt;
				else pitch-=pitchspeed*dt;
			}
			if (rpm>regspeed*1.11)
			{
				pitch-=pitchspeed*dt;
			}
		}
	}
	if (!failed)
	{
		axis=nodes[noderef].RelPosition-nodes[nodeback].RelPosition;
		axis.normalise();
	}
	//estimate amount of energy
	float estrotenergy=0.5*numblades*nodes[nodep[0]].mass*radius*radius*(rpm/9.549)*(rpm/9.549);
	//for each blade
	float totthrust=0;
	float tottorque=0;
	for (int i=0; i<numblades; i++)
	{
		float dragfactor=0.01;
		if (!failed)
		{
			Vector3 totaltipforce=Vector3::ZERO;
			//span vector, left to right
			Vector3 spanv=(nodes[nodep[i]].RelPosition-nodes[noderef].RelPosition);
			spanv.normalise();
			//chord vector, front to back
			Vector3 refchordv=-axis.crossProduct(spanv);
			//grab this for propulsive forces
			Vector3 tipf=-refchordv;
			totaltipforce+=(tipforce-rpm/10.0)*tipf; //add a bit of mechanical friction
			//for each blade segment (there are 6 elements)
			for (int j=0; j<5; j++) //outer to inner, the 6th blade element is ignored
			{
				//proportion
				float proport=((float)j+0.5)/6.0;
				//evaluate wind direction
				Vector3 wind=-(nodes[nodep[i]].Velocity*(1.0-proport)+nodes[noderef].Velocity*proport);
				float wspeed=wind.length();

				Vector3 liftv=spanv.crossProduct(-wind);
				liftv.normalise();
				//rotate according to pitch
				Vector3 chordv=Quaternion(Degree(pitch+twistmap[j]-7.0), spanv)*refchordv;
				//wing normal
				Vector3 normv=chordv.crossProduct(spanv);
				//calculate angle of attack
				Vector3 pwind=Plane(Vector3::ZERO, normv, chordv).projectVector(wind);
				Vector3 dumb;
				Degree daoa;
				chordv.getRotationTo(pwind).ToAngleAxis(daoa, dumb);
				float aoa=daoa.valueDegrees();
				float raoa=daoa.valueRadians();
				if (dumb.dotProduct(spanv)>0) {aoa=-aoa; raoa=-raoa;};
				//get airfoil data
				float cz, cx, cm;
				airfoil->getparams(aoa, 1.0, 0.0, &cz, &cx, &cm);
				//surface computation
				float s=radius*bladewidth/6.0;

				//drag
				//wforce=(8.0*cx+cx*cx/(3.14159*radius/0.4))*0.5*airdensity*wspeed*s*wind;
				Vector3 eforce=(4.0*cx+cx*cx/(3.14159*radius/bladewidth))*0.5*airdensity*wspeed*s*wind;
				//lift
				float lift=cz*0.5*airdensity*wspeed*wspeed*s;
				eforce+=lift*liftv;
				totthrust+=eforce.dotProduct(axis);

				//apply forces
				nodes[noderef].Forces+=eforce*proport;
				totaltipforce+=eforce*(1.0-proport);

//				if (i==0) sprintf(debug, "rend %.4i%%, wind %.3i kts, aoa %.3i, power %.5ihp, thrust %.6ilbf, torque %.6ilbft, pitch %.3i, propwash %.3i kts", (int)(100.0*wforce.dotProduct(axis)*numblades*nodes[noderef].Velocity.length()/(rpm*0.10471976*2.0*3.14159*enginecouple)), (int)(wspeed*1.9438), (int)aoa, (int)(enginepower*1.34), (int)(wforce.dotProduct(axis)*numblades*0.2248), (int)(enginecouple/1.35582), (int)pitch, (int)(propwash*1.9438));
				//			if (i==0) sprintf(debug, "lfx=%f lfy=%f lfz=%f vx=%f vy=%f vz=%f cx=%f cz=%f lift=%f", liftv.x, liftv.y, liftv.z, wind.x, wind.y, wind.z, cx, cz, lift);
			}
			tottorque+=tipf.dotProduct(totaltipforce)*radius;
			//correct amount of energy
			float correctfactor=0;
			if (rpm>100) correctfactor=(rotenergy-estrotenergy)/(numblades*radius*dt*rpm/9.549);
			if (correctfactor>1000.0) correctfactor=1000.0;
			if (correctfactor<-1000.0) correctfactor=-1000.0;
			nodes[nodep[i]].Forces+=totaltipforce+correctfactor*tipf;
		}
		else
		{
			//failed case
			//add drag
			Vector3 wind=-nodes[nodep[i]].Velocity;
			float wspeed=wind.length();
			nodes[nodep[i]].Forces+=airdensity*wspeed*wind;
		}
	}
	//compute the next energy level
	rotenergy+=(double)tottorque*dt*rpm/9.549;
//	sprintf(debug, "pitch %i thrust %i totenergy=%i apparentenergy=%i", (int)pitch, (int)totthrust, (int)rotenergy, (int)estrotenergy);
	//prop wash
	float speed=nodes[noderef].Velocity.length();
	float thrsign=1.0;
	if (totthrust<0) {thrsign=-0.1; totthrust=-totthrust;};
	if (!failed) propwash=thrsign*sqrt(totthrust/(0.5*airdensity*proparea)+speed*speed)-speed; else propwash=0;
	if (propwash<0) propwash=0;
}

void Turboprop::setThrotle(float val)
{
	if (val>1.0) val=1.0;
	if (val<0.0) val=0.0;
	throtle=val;
#ifdef USE_OPENAL
	//sound update
	if(ssm) ssm->modulate(trucknum, thr_id, val);
#endif //OPENAL
}

float Turboprop::getThrotle()
{
	return throtle;
}

void Turboprop::setRPM(float _rpm)
{
	rpm = _rpm;
}

void Turboprop::reset()
{
	rpm=0;
	throtle=0;
	failed=false;
	ignition=false;
	reverse=false;
	pitch=0;
	rotenergy=0;
}

void Turboprop::toggleReverse()
{
	throtle=0;
	reverse=!reverse;
	pitch=0;
}

void Turboprop::flipStart()
{
	if (timer-lastflip<0.3) return;
	ignition=!ignition;
	if (ignition && !failed)
	{
		warmup=true;
		warmupstart=timer;
#ifdef USE_OPENAL
		if(ssm) ssm->trigStart(trucknum, src_id);
#endif //OPENAL
	}
	else
	{
#ifdef USE_OPENAL
		if(ssm) ssm->trigStop(trucknum, src_id);
#endif //OPENAL
	}
	lastflip=timer;
}

void Turboprop::addPale(SceneNode* sn)
{
	vpales[free_vpale]=sn;
	free_vpale++;
}

void Turboprop::addSpinner(SceneNode* sn)
{
	vspinner=sn;
}

