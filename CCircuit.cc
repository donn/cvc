/*
 * CCircuit.cc
 *
 * Copyright 2014-2106 D. Mitch Bailey  cvc at shuharisystem dot com
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

#include "CCircuit.hh"

#include "CDevice.hh"
#include "CFixedText.hh"

void CCircuit::AddPortSignalIds(CTextList * thePortList_p) {
	for (CTextList::iterator text_pit = thePortList_p->begin(); text_pit != thePortList_p->end(); ++text_pit) {
		localSignalIdMap[*text_pit] = portCount++;
	}
}

void CCircuit::SetSignalIds(CTextList * signalList_p, CNetIdVector& signalId_v ) {
	netId_t mySignalId;

	signalId_v.reserve(signalList_p->size());
	for (CTextList::iterator text_pit = signalList_p->begin(); text_pit != signalList_p->end(); ++text_pit) {
		try {
			mySignalId = localSignalIdMap.at(*text_pit);
		}
		catch (const out_of_range& oor_exception) {
			mySignalId = localSignalIdMap.size();
			localSignalIdMap[*text_pit] = mySignalId;
			internalSignalList.push_back(*text_pit);
		}
		signalId_v.push_back(mySignalId);
	}
}

void CCircuit::LoadDevices(CDevicePtrList * theDevicePtrList_p) {
	deviceId_t	mySubcircuitId;
	deviceId_t	myDeviceId;
	CDevice *	myDevice_p;
	static int myDeviceCount = 0;
	static int myCircuitCount = 0;
	static int myInstanceCount = 0;
	static int myPrintCount = 0;

	devicePtr_v.reserve(theDevicePtrList_p->DeviceCount());
	deviceErrorCount_v.resize(theDevicePtrList_p->DeviceCount(), 0);
	subcircuitPtr_v.reserve(theDevicePtrList_p->SubcircuitCount());
	myCircuitCount++;
	if ( ++myPrintCount >= 100000 ) {
			cout << "Read " << myCircuitCount << " circuits, " << myInstanceCount << " instances, " << myDeviceCount << " devices\r" << std::flush;
			myPrintCount = 0;
	}
	for (CDevicePtrList::iterator device_ppit = theDevicePtrList_p->begin(); device_ppit != theDevicePtrList_p->end(); device_ppit++) {
		myDevice_p = *device_ppit;
		SetSignalIds(myDevice_p->signalList_p, myDevice_p->signalId_v);
//		delete myDevice_p->signalList_p;
		myDevice_p->parent_p = this;
		if ( localDeviceIdMap.count(myDevice_p->name) ) {
			cout << "ERROR: Duplicate instance " << myDevice_p->name << " in " << name << endl;
			throw EDuplicateInstance();
		}
		if ( myDevice_p->IsSubcircuit() ) {
			mySubcircuitId = subcircuitPtr_v.size();
			subcircuitPtr_v.push_back(myDevice_p);
			localDeviceIdMap[myDevice_p->name] = mySubcircuitId;
			myInstanceCount++;
			myPrintCount++;
		} else {
			myDeviceId = devicePtr_v.size();
			devicePtr_v.push_back(myDevice_p);
			localDeviceIdMap[myDevice_p->name] = myDeviceId;
			myDeviceCount++;
			myPrintCount++;
		}
/*
	while( ! theDevicePtrList->empty() ) {
		SetSignalIds(theDevicePtrList->front()->signalList_p, theDevicePtrList->front()->signalId_v);
		delete theDevicePtrList->front()->signalList_p;
		theDevicePtrList->front()->parent_p = this;
		if ( theDevicePtrList->front()->IsSubcircuit() ) {
			mySubcircuitId = subcircuitPtr_v.size();
			subcircuitPtr_v.push_back(theDevicePtrList->front());
			localDeviceIdMap[theDevicePtrList->front()->name] = mySubcircuitId;
		} else {
			myDeviceId = devicePtr_v.size();
			devicePtr_v.push_back(theDevicePtrList->front());
			localDeviceIdMap[theDevicePtrList->front()->name] = myDeviceId;
		}
		theDevicePtrList->pop_front();
*/
	}
	// list to vector conversion (top circuit must be redone to include top ports)
	internalSignal_v.reserve(internalSignalList.size());
	while( ! internalSignalList.empty() ) {
		internalSignal_v.push_back(internalSignalList.front());
		internalSignalList.pop_front();
	}

}

void CCircuit::CountObjectsAndLinkSubcircuits(unordered_map<text_t, CCircuit *> & theCircuitNameMap) {
	CCircuit * myChild_p;

	deviceCount = devicePtr_v.size();
	netCount = LocalNetCount();
	subcircuitCount = subcircuitPtr_v.size();
	for (CDevicePtrVector::iterator subcircuit_ppit = subcircuitPtr_v.begin(); subcircuit_ppit != subcircuitPtr_v.end(); subcircuit_ppit++) {
		try {
			(*subcircuit_ppit)->master_p = theCircuitNameMap.at((*subcircuit_ppit)->masterName);
			myChild_p = (*subcircuit_ppit)->master_p;
			if ( myChild_p->linked ) {
				if ( myChild_p->subcircuitCount > 0 ) {
					myChild_p->CountInstantiations();
				}
			} else {
				myChild_p->CountObjectsAndLinkSubcircuits(theCircuitNameMap);
			}
//			assert((*subcircuit_ppit)->signalId_v.size() == myChild_p->portCount);
			if ( (*subcircuit_ppit)->signalId_v.size() != myChild_p->portCount ) {
				stringstream myErrorMessage;
				myErrorMessage << "port mismatch in " << (*subcircuit_ppit)->name << " " << (*subcircuit_ppit)->signalId_v.size() << ":" << myChild_p->portCount << endl;
				throw EFatalError(myErrorMessage.str());
			}
			myChild_p->instanceCount++;
			deviceCount += myChild_p->deviceCount;
			netCount += myChild_p->netCount;
			subcircuitCount += myChild_p->subcircuitCount;
		}
		catch (const out_of_range& oor_exception) {
			stringstream myErrorMessage;
			myErrorMessage << "could not find subcircuit: " << (*subcircuit_ppit)->name << "(" << (*subcircuit_ppit)->masterName << ") in " << name << endl;
			throw EFatalError(myErrorMessage.str());
		}
	}
	linked = true;
}

void CCircuit::CountInstantiations() {
	CCircuit * myChild_p;

	for (CDevicePtrVector::iterator subcircuit_ppit = subcircuitPtr_v.begin(); subcircuit_ppit != subcircuitPtr_v.end(); subcircuit_ppit++) {
		myChild_p = (*subcircuit_ppit)->master_p;
		assert(myChild_p->linked);
		if ( myChild_p->subcircuitCount > 0 ) {
			myChild_p->CountInstantiations();
		}
		myChild_p->instanceCount++;
	}
}

void CCircuit::Print (const string theIndentation) {
	CTextVector mySignalName_v;
	netId_t mySignalId = 0;

	cout << theIndentation << "Circuit Name: " << name << endl;
	cout << theIndentation << "Linked: " << linked;
	cout << "  Counts Devices: " << deviceCount;
	cout << "  Nets: " << netCount;
	cout << "  Subcircuits: " << subcircuitCount;
	cout << "  Instances: " << instanceCount << endl;
	string myIndentation = theIndentation + " ";
	cout << myIndentation << "Signal List(" << portCount << "/" << localSignalIdMap.size() << "): ";
	mySignalName_v.reserve(localSignalIdMap.size());
	for (CTextNetIdMap::iterator textNetIdPair_pit = localSignalIdMap.begin(); textNetIdPair_pit != localSignalIdMap.end(); ++textNetIdPair_pit) {
		mySignalName_v[textNetIdPair_pit->second] = textNetIdPair_pit->first;
	}
	while (mySignalId < localSignalIdMap.size()) {
		cout << " " << mySignalName_v[mySignalId] << ":" << mySignalId;
		mySignalId++;
	}
	cout << endl;
	devicePtr_v.Print(mySignalName_v, myIndentation, "DeviceList>");
	subcircuitPtr_v.Print(mySignalName_v, localDeviceIdMap, myIndentation, "SubcircuitList>");
	if ( instanceCount > 0 && instanceId_v.size() == instanceCount ) {
		cout << myIndentation << "InstanceList>";
		for (instanceId_t instance_it = 0; instance_it < instanceCount; instance_it++) {
			cout << " " << instanceId_v[instance_it];
		}
		cout << endl;
	}
	cout << endl;
}

void CCircuitPtrList::Clear() {
	cdlText.Clear();
	circuitNameMap.clear();
	parameterText.Clear();
	errorCount = 0;
}

void CCircuitPtrList::Print (const string theIndentation, const string theHeading) {
	cout << theIndentation << theHeading << " start" << endl;
	string myIndentation = theIndentation + " ";
	cout << myIndentation << "Cdl text size: " << cdlText.Size() << endl;
	for (CCircuitPtrList::iterator circuit_ppit = begin(); circuit_ppit != end(); ++circuit_ppit) {
		(*circuit_ppit)->Print(myIndentation);
	}
	cout << theIndentation << theHeading << " end" << endl << endl;
}

/*
void CCircuitPtrList::CreateDatabase(const string theTopCircuitName) {
	CCircuit * myTopCircuit = FindCircuit(theTopCircuitName);
	myTopCircuit->CountObjectsAndLinkSubcircuits(circuitNameMap);
	myTopCircuit->netCount += myTopCircuit->portCount;
	myTopCircuit->subcircuitCount++;
	myTopCircuit->instanceCount++;
//	myTopCircuit->AssignGlobalId();
}
*/

CCircuit * CCircuitPtrList::FindCircuit(const string theSearchCircuitName) {
	return(circuitNameMap.at(cdlText.GetTextAddress(theSearchCircuitName)));
/*  move error handling to calling routine
	try {
		return(circuitNameMap.at(cdlText.GetTextAddress(theSearchCircuitName)));
	}
	catch (const out_of_range& oor_exception) {
//		cout << "** ERROR: could not find subcircuit: " << theSearchCircuitName << endl;
		throw EFatalError("could not find subcircuit: " + theSearchCircuitName);
//		exit(1);
	}
*/
}

void CCircuitPtrList::PrintAndResetCircuitErrors(deviceId_t theErrorLimit, ogzstream & theErrorFile) {
	list<string> myErrorSummaryList;
	for (auto circuit_ppit = begin(); circuit_ppit != end(); circuit_ppit++) {
/*
		if ( theErrorLimit > 0 && (*circuit_ppit)->errorCount > theErrorLimit ) {
			cout << "INFO: SUBCKT " << (*circuit_ppit)->name << " error count " << (*circuit_ppit)->errorCount << "/" << (*circuit_ppit)->instanceId_v.size();
			cout << " exceeded error limit " << theErrorLimit << endl;
		}
		(*circuit_ppit)->errorCount = 0;
*/
		for (size_t device_it = 0; device_it < (*circuit_ppit)->devicePtr_v.size(); device_it++) {
			if ( (*circuit_ppit)->deviceErrorCount_v[device_it] > 0 ) {
				stringstream myErrorSummary;
				myErrorSummary.str("");
				myErrorSummary << "INFO: SUBCKT (" << (*circuit_ppit)->name << ")/" << (*circuit_ppit)->devicePtr_v[device_it]->name;
				myErrorSummary << "(" << (*circuit_ppit)->devicePtr_v[device_it]->model_p->name << ")";
				myErrorSummary << " error count " << (*circuit_ppit)->deviceErrorCount_v[device_it] << "/" << (*circuit_ppit)->instanceId_v.size();
				if ( theErrorLimit > 0 && (*circuit_ppit)->deviceErrorCount_v[device_it] > theErrorLimit ) {
	//				theErrorFile << "INFO: SUBCKT " << (*circuit_ppit)->name << "/" << (*circuit_ppit)->devicePtr_v[device_it]->name << " error count ";
	//				theErrorFile << (*circuit_ppit)->deviceErrorCount_v[device_it] << "/" << (*circuit_ppit)->instanceId_v.size();
					myErrorSummary << " exceeded error limit " << theErrorLimit;
				}
				(*circuit_ppit)->deviceErrorCount_v[device_it] = 0;
				myErrorSummaryList.push_front(myErrorSummary.str());
			}
		}
	}
	myErrorSummaryList.sort();
	for ( auto error_pit = myErrorSummaryList.begin(); error_pit != myErrorSummaryList.end(); error_pit++ ) {
		theErrorFile << (*error_pit) << endl;
	}
	theErrorFile << endl;
	theErrorFile << "! Finished" << endl << endl;
}
