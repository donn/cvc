/*
 * CInstance.cc
 *
 * Copyright 2014-2018 D. Mitch Bailey  cvc at shuharisystem dot com
 *
 * This file is part of cvc.
 *
 * cvc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cvc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cvc.  If not, see <http://www.gnu.org/licenses/>.
 *
 * You can download cvc from https://github.com/d-m-bailey/cvc.git
 */

#include "CInstance.hh"

#include "CCvcDb.hh"
#include "CCircuit.hh"
#include <obstack.h>

#define NOT_PARALLEL false

int gHashCollisionCount;
int gMaxHashLength;
 
void CInstance::AssignTopGlobalIDs(CCvcDb * theCvcDb_p, CCircuit * theMaster_p) {

	theMaster_p->instanceId_v.push_back(0);
	theCvcDb_p->subcircuitCount = 1;
	firstSubcircuitId = 1;
	parentId = 0;
	master_p = theMaster_p;
	netId_t myLastNet = theMaster_p->localSignalIdMap.size();
	localToGlobalNetId_v.reserve(myLastNet);
	localToGlobalNetId_v.resize(myLastNet);
	theMaster_p->internalSignal_v.clear();
	theMaster_p->internalSignal_v.reserve(theMaster_p->localSignalIdMap.size());
	theMaster_p->internalSignal_v.resize(theMaster_p->localSignalIdMap.size());
	for (CTextNetIdMap::iterator textNetIdPair_pit = theMaster_p->localSignalIdMap.begin(); textNetIdPair_pit != theMaster_p->localSignalIdMap.end(); textNetIdPair_pit++) {
		theMaster_p->internalSignal_v[textNetIdPair_pit->second] = textNetIdPair_pit->first;
	}
	for (netId_t globalNet_it = 0; globalNet_it < myLastNet; globalNet_it++) {
		localToGlobalNetId_v[globalNet_it] = globalNet_it;
	}
	theCvcDb_p->netCount = myLastNet;
	theCvcDb_p->subcircuitCount += theMaster_p->subcircuitPtr_v.size();
	theCvcDb_p->deviceCount = theMaster_p->devicePtr_v.size();
	theCvcDb_p->netParent_v.resize(theCvcDb_p->netCount, 0);
	theCvcDb_p->deviceParent_v.resize(theCvcDb_p->deviceCount, 0);

	instanceId_t myLastSubcircuitId = theMaster_p->subcircuitPtr_v.size();
	instanceId_t myNewInstanceId;
	for (instanceId_t subcircuit_it = 0; subcircuit_it < myLastSubcircuitId; subcircuit_it++) {
		//if (theMaster_p->subcircuitPtr_v[subcircuit_it]->master_p->deviceCount > 0) {
			myNewInstanceId = firstSubcircuitId + subcircuit_it;
			CDevice * mySubcircuit_p = theMaster_p->subcircuitPtr_v[subcircuit_it];
			if ( mySubcircuit_p->master_p->instanceId_v.size() == 0 ) mySubcircuit_p->master_p->AllocateInstances(theCvcDb_p, myNewInstanceId);
			theCvcDb_p->instancePtr_v[myNewInstanceId] = new CInstance;
			theCvcDb_p->instancePtr_v[myNewInstanceId]->AssignGlobalIDs(theCvcDb_p, myNewInstanceId, mySubcircuit_p, 0, this, NOT_PARALLEL);
		//}
	}
	theCvcDb_p->netParent_v.shrink_to_fit();
	theCvcDb_p->deviceParent_v.shrink_to_fit();
	theCvcDb_p->debugFile << "DEBUG: netParent size " << theCvcDb_p->netParent_v.size() << "; deviceParent size " << theCvcDb_p->deviceParent_v.size() << endl;
	theCvcDb_p->debugFile << "DEBUG: parallel collisions " << gHashCollisionCount << " max length " << gMaxHashLength << endl;
}

void CInstance::AssignGlobalIDs(CCvcDb * theCvcDb_p, const instanceId_t theInstanceId, CDevice * theSubcircuit_p, const instanceId_t theParentId,
		CInstance * theParent_p, bool isParallel) {
	master_p = theSubcircuit_p->master_p;
	instanceId_t myLastSubcircuitId = master_p->subcircuitPtr_v.size();
	firstSubcircuitId = theCvcDb_p->subcircuitCount;
	master_p->instanceId_v.push_back(theInstanceId);

	parentId = theParentId;
	instanceId_t myParallelInstance;
	if ( isParallel ) {
		assert(theParent_p->IsParallelInstance());
		CInstance * myParentParallelInstance_p = theCvcDb_p->instancePtr_v[theParent_p->parallelInstanceId];
		int myInstanceOffset = theInstanceId - theParent_p->firstSubcircuitId;
		parallelInstanceId = myParentParallelInstance_p->firstSubcircuitId + myInstanceOffset;
		CInstance * myParallelInstance_p = theCvcDb_p->instancePtr_v[parallelInstanceId];
		if ( myParallelInstance_p->IsParallelInstance() ) {
			theCvcDb_p->instancePtr_v[myParallelInstance_p->parallelInstanceId]->parallelInstanceCount++;
		} else {
			myParallelInstance_p->parallelInstanceCount++;
		}
		theCvcDb_p->debugFile << "DEBUG: found parallel instance in parallel instance at " << theCvcDb_p->HierarchyName(theInstanceId) << endl;
	} else {
		if ( theSubcircuit_p->signalId_v.size() <= theCvcDb_p->cvcParameters.cvcParallelCircuitPortLimit ) {
			myParallelInstance = theSubcircuit_p->FindParallelInstance(theCvcDb_p, theInstanceId, theParent_p->localToGlobalNetId_v);
			if ( myParallelInstance == theInstanceId ) {
				theCvcDb_p->instancePtr_v[theInstanceId]->parallelInstanceCount = 1;
			} else {  // skip parallel circuits
				theCvcDb_p->instancePtr_v[myParallelInstance]->parallelInstanceCount++;
				theCvcDb_p->debugFile << "DEBUG: found parallel instance at " << theCvcDb_p->HierarchyName(theInstanceId) << endl;
				parallelInstanceId = myParallelInstance;
				isParallel = true;
			}
		}
	}
	if ( ! isParallel ) {  // don't expand devices/nets for parallel instances
		firstNetId = theCvcDb_p->netCount;
		firstDeviceId = theCvcDb_p->deviceCount;

		netId_t myLastNet = master_p->localSignalIdMap.size();
		localToGlobalNetId_v.reserve(myLastNet);
		localToGlobalNetId_v.resize(myLastNet);
		for (netId_t net_it = 0; net_it < master_p->portCount; net_it++) {
			localToGlobalNetId_v[net_it] = theParent_p->localToGlobalNetId_v[theSubcircuit_p->signalId_v[net_it]];
		}
		for (netId_t net_it = master_p->portCount; net_it < myLastNet; net_it++) {
			localToGlobalNetId_v[net_it] = firstNetId + net_it - master_p->portCount;
		}
		theCvcDb_p->netCount += master_p->LocalNetCount();
		theCvcDb_p->deviceCount += master_p->devicePtr_v.size();
		theCvcDb_p->netParent_v.resize(theCvcDb_p->netCount, theInstanceId);
		theCvcDb_p->deviceParent_v.resize(theCvcDb_p->deviceCount, theInstanceId);
	}

	theCvcDb_p->subcircuitCount += master_p->subcircuitPtr_v.size();

	instanceId_t myNewInstanceId;
	for (instanceId_t subcircuit_it = 0; subcircuit_it < myLastSubcircuitId; subcircuit_it++) {
		myNewInstanceId = firstSubcircuitId + subcircuit_it;
		CDevice * mySubcircuit_p = master_p->subcircuitPtr_v[subcircuit_it];
		//if (mySubcircuit_p->master_p->deviceCount > 0) {
			if ( mySubcircuit_p->master_p->instanceId_v.size() == 0 ) mySubcircuit_p->master_p->AllocateInstances(theCvcDb_p, myNewInstanceId);
			theCvcDb_p->instancePtr_v[myNewInstanceId] = new CInstance;
			theCvcDb_p->instancePtr_v[myNewInstanceId]->AssignGlobalIDs(theCvcDb_p, myNewInstanceId, mySubcircuit_p, theInstanceId, this, isParallel);
		//}
	}
}

void CInstance::Print (const instanceId_t theInstanceId, const string theIndentation) {
	cout << theIndentation << "Instance> " << theInstanceId << endl;
	string myIndentation = theIndentation + " ";
	cout << myIndentation << "First: subcircuit: " << firstSubcircuitId;
	cout << "  net: " << firstNetId;
	cout << "  device: " << firstDeviceId << endl;
	cout << myIndentation << "parent: " << parentId << "  master: " << master_p->name << endl;
	cout << myIndentation << "signal map:";
	for (netId_t net_it = 0; net_it < localToGlobalNetId_v.size(); net_it++) {
		cout << " " << net_it << ":" << localToGlobalNetId_v[net_it];
	}
	cout << endl;
}

void CInstancePtrVector::Clear() {
	for ( auto instance_ppit = begin(); instance_ppit != end(); instance_ppit++ ) {
		delete (*instance_ppit);
	}
	resize(0);
}

CInstancePtrVector::~CInstancePtrVector() {
	Clear();
}

