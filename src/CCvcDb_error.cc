/*
 * CCvcDb_error.cc
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

#include "CCvcDb.hh"

#include "CCircuit.hh"
#include "CConnectionCount.hh"
#include "CModel.hh"
#include "CDevice.hh"
#include "CPower.hh"
#include "CInstance.hh"
#include "CEventQueue.hh"
#include "CVirtualNet.hh"

void CCvcDb::PrintFuseError(netId_t theTargetNetId, CConnection & theConnections) {
	errorCount[FUSE_ERROR]++;
	if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(theConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
		CFullConnection myFullConnections;
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[theConnections.deviceId]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[theConnections.deviceId - myInstance_p->firstDeviceId];
		MapDeviceNets(myInstance_p, myDevice_p, myFullConnections);
		errorFile << "! Power/Ground path through fuse at " << NetName(theTargetNetId, PRINT_CIRCUIT_ON, PRINT_HIERARCHY_OFF);
		if ( deviceType_v[theConnections.deviceId] == FUSE_ON ) {
			errorFile << "; possible floating when cut" << endl;
		} else if ( deviceType_v[theConnections.deviceId] == FUSE_OFF ) {
			errorFile << "; possibly unusable" << endl;
		} else {
			errorFile << "; unknown fuse type" << endl;
		}
		PrintDeviceWithMinMaxConnections(deviceParent_v[theConnections.deviceId], myFullConnections, errorFile);
		errorFile << endl;
	}
}

void CCvcDb::PrintMinVoltageConflict(netId_t theTargetNetId, CConnection & theMinConnections, voltage_t theExpectedVoltage, float theLeakCurrent) {
	errorCount[MIN_VOLTAGE_CONFLICT]++;
	if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(theMinConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
		CFullConnection myFullConnections;
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[theMinConnections.deviceId]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[theMinConnections.deviceId - myInstance_p->firstDeviceId];
		MapDeviceNets(myInstance_p, myDevice_p, myFullConnections);
		errorFile << "! Min voltage already set for " << NetName(theTargetNetId, PRINT_CIRCUIT_ON, PRINT_HIERARCHY_OFF);
		errorFile << " at mos diode: expected/found " << theExpectedVoltage << "/" << theMinConnections.gateVoltage;
		errorFile << " estimated current " << AddSiSuffix(theLeakCurrent) << "A" << endl;
		PrintDeviceWithMinMaxConnections(deviceParent_v[theMinConnections.deviceId], myFullConnections, errorFile);
		errorFile << endl;
	}
}

void CCvcDb::PrintMaxVoltageConflict(netId_t theTargetNetId, CConnection & theMaxConnections, voltage_t theExpectedVoltage, float theLeakCurrent) {
	errorCount[MAX_VOLTAGE_CONFLICT]++;
	if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(theMaxConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
		CFullConnection myFullConnections;
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[theMaxConnections.deviceId]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[theMaxConnections.deviceId - myInstance_p->firstDeviceId];
		MapDeviceNets(myInstance_p, myDevice_p, myFullConnections);
		errorFile << "! Max voltage already set for " << NetName(theTargetNetId, PRINT_CIRCUIT_ON, PRINT_HIERARCHY_OFF);
		errorFile << " at mos diode: expected/found " << theExpectedVoltage << "/" << theMaxConnections.gateVoltage;
		errorFile << " estimated current " << AddSiSuffix(theLeakCurrent) << "A" << endl;
		PrintDeviceWithMinMaxConnections(deviceParent_v[theMaxConnections.deviceId], myFullConnections, errorFile);
		errorFile << endl;
	}
}

string CCvcDb::FindVbgError(voltage_t theParameter, CFullConnection & theConnections) {
	if ( ( theConnections.validMaxGate && theConnections.validMinBulk
				&& abs(theConnections.maxGateVoltage - theConnections.minBulkVoltage) > theParameter )
			|| ( theConnections.validMinGate && theConnections.validMaxBulk
				&& abs(theConnections.minGateVoltage - theConnections.maxBulkVoltage) > theParameter ) ) {
		return("Overvoltage Error:Gate vs Bulk:");
	} else if ( ! cvcParameters.cvcLeakOvervoltage ) {
		return ("");
	} else if ( ( theConnections.validMaxGateLeak && theConnections.validMinBulkLeak
				&& abs(theConnections.maxGateLeakVoltage - theConnections.minBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMinGateLeak && theConnections.validMaxBulkLeak
				&& abs(theConnections.minGateLeakVoltage - theConnections.maxBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMaxGateLeak && ! theConnections.validMinBulkLeak
				&& abs(theConnections.maxGateLeakVoltage) > theParameter )
			|| ( theConnections.validMinGateLeak && ! theConnections.validMaxBulkLeak
				&& abs(theConnections.minGateLeakVoltage) > theParameter )
			|| ( ! theConnections.validMaxGateLeak && theConnections.validMinBulkLeak
				&& abs(theConnections.minBulkLeakVoltage) > theParameter )
			|| ( ! theConnections.validMinGate && theConnections.validMaxBulkLeak
				&& abs(theConnections.maxBulkLeakVoltage) > theParameter ) ) {
		return("Overvoltage Error:Gate vs Bulk: (logic ok)");
	}
	return("");
}

string CCvcDb::FindVbsError(voltage_t theParameter, CFullConnection & theConnections) {
	if ( ( theConnections.validMaxSource && theConnections.validMinBulk
				&& abs(theConnections.maxSourceVoltage - theConnections.minBulkVoltage) > theParameter )
			|| ( theConnections.validMinSource && theConnections.validMaxBulk
				&& abs(theConnections.minSourceVoltage - theConnections.maxBulkVoltage) > theParameter )
			|| ( theConnections.validMaxDrain && theConnections.validMinBulk
				&& abs(theConnections.maxDrainVoltage - theConnections.minBulkVoltage) > theParameter )
			|| ( theConnections.validMinDrain && theConnections.validMaxBulk
				&& abs(theConnections.minDrainVoltage - theConnections.maxBulkVoltage) > theParameter ) ) {
		return("Overvoltage Error:Source/Drain vs Bulk:");
	} else if ( ! cvcParameters.cvcLeakOvervoltage ) {
		return ("");
	} else if ( ( theConnections.validMaxSourceLeak && theConnections.validMinBulkLeak
				&& abs(theConnections.maxSourceLeakVoltage - theConnections.minBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMinSourceLeak && theConnections.validMaxBulkLeak
				&& abs(theConnections.minSourceLeakVoltage - theConnections.maxBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMaxDrainLeak && theConnections.validMinBulkLeak
				&& abs(theConnections.maxDrainLeakVoltage - theConnections.minBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMinDrainLeak && theConnections.validMaxBulkLeak
				&& abs(theConnections.minDrainLeakVoltage - theConnections.maxBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMinBulkLeak && ! theConnections.validMaxSourceLeak && ! theConnections.validMaxDrainLeak
				&& abs(theConnections.minBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMaxBulkLeak && ! theConnections.validMinSourceLeak && ! theConnections.validMinDrainLeak
				&& abs(theConnections.maxBulkLeakVoltage) > theParameter )
			|| ( theConnections.validMinSourceLeak && ! theConnections.validMaxBulkLeak
				&& abs(theConnections.minSourceLeakVoltage) > theParameter )
			|| ( theConnections.validMaxSourceLeak && ! theConnections.validMinBulkLeak
				&& abs(theConnections.maxSourceLeakVoltage) > theParameter )
			|| ( theConnections.validMinDrainLeak && ! theConnections.validMaxBulkLeak
				&& abs(theConnections.minDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMaxDrainLeak && ! theConnections.validMinBulkLeak
				&& abs(theConnections.maxDrainLeakVoltage) > theParameter ) ) {
		return("Overvoltage Error:Source/Drain vs Bulk: (logic ok)");
	}
	return("");
}

string CCvcDb::FindVdsError(voltage_t theParameter, CFullConnection & theConnections) {
	if ( ( theConnections.validMinSource && theConnections.validMaxDrain
				&& abs(theConnections.minSourceVoltage - theConnections.maxDrainVoltage) > theParameter )
			|| ( theConnections.validMaxSource && theConnections.validMinDrain
				&& abs(theConnections.maxSourceVoltage - theConnections.minDrainVoltage) > theParameter ) ) {
		if ( theConnections.IsPumpCapacitor() &&
				abs(theConnections.minSourceVoltage - theConnections.minDrainVoltage) <= theParameter &&
				abs(theConnections.maxSourceVoltage - theConnections.maxDrainVoltage) <= theParameter ) {
			; // for pumping capacitors only check min-min/max-max differences
		} else {
			return("Overvoltage Error:Source vs Drain:");
		}
	} else if ( ! cvcParameters.cvcLeakOvervoltage ) {
		return ("");
	} else if ( ( theConnections.validMinSourceLeak && theConnections.validMaxDrainLeak
				&& abs(theConnections.minSourceLeakVoltage - theConnections.maxDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMaxSourceLeak && theConnections.validMinDrainLeak
				&& abs(theConnections.maxSourceLeakVoltage - theConnections.minDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMinSourceLeak && ! theConnections.validMaxDrainLeak
				&& abs(theConnections.minSourceLeakVoltage) > theParameter )
			|| ( ! theConnections.validMinSourceLeak && theConnections.validMaxDrainLeak
				&& abs(theConnections.maxDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMaxSourceLeak && ! theConnections.validMinDrainLeak
				&& abs(theConnections.maxSourceLeakVoltage) > theParameter )
			|| ( ! theConnections.validMaxSourceLeak && theConnections.validMinDrainLeak
				&& abs(theConnections.minDrainLeakVoltage) > theParameter )	) {
		return("Overvoltage Error:Source vs Drain: (logic ok)");
	}
	return("");
}

string CCvcDb::FindVgsError(voltage_t theParameter, CFullConnection & theConnections) {
	if ( ( theConnections.validMinGate && theConnections.validMaxSource
				&& abs(theConnections.minGateVoltage - theConnections.maxSourceVoltage) > theParameter )
			|| ( theConnections.validMaxGate && theConnections.validMinSource
				&& abs(theConnections.maxGateVoltage - theConnections.minSourceVoltage) > theParameter )
			|| ( theConnections.validMinGate && theConnections.validMaxDrain
				&& abs(theConnections.minGateVoltage - theConnections.maxDrainVoltage) > theParameter )
			|| ( theConnections.validMaxGate && theConnections.validMinDrain
				&& abs(theConnections.maxGateVoltage - theConnections.minDrainVoltage) > theParameter ) ) {
		return("Overvoltage Error:Gate vs Source/Drain:");
	} else if ( ! cvcParameters.cvcLeakOvervoltage ) {
		return ("");
	} else if ( ( theConnections.validMinGateLeak && theConnections.validMaxSourceLeak
				&& abs(theConnections.minGateLeakVoltage - theConnections.maxSourceLeakVoltage) > theParameter )
			|| ( theConnections.validMaxGateLeak && theConnections.validMinSourceLeak
				&& abs(theConnections.maxGateLeakVoltage - theConnections.minSourceLeakVoltage) > theParameter )
			|| ( theConnections.validMinGateLeak && theConnections.validMaxDrainLeak
				&& abs(theConnections.minGateLeakVoltage - theConnections.maxDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMaxGateLeak && theConnections.validMinDrainLeak
				&& abs(theConnections.maxGateLeakVoltage - theConnections.minDrainLeakVoltage) > theParameter )
			|| ( theConnections.validMinGateLeak && ! theConnections.validMaxSourceLeak && ! theConnections.validMaxDrainLeak
				&& abs(theConnections.minGateLeakVoltage) > theParameter )
			|| ( theConnections.validMaxGateLeak && ! theConnections.validMinSourceLeak && ! theConnections.validMinDrainLeak
				&& abs(theConnections.maxGateLeakVoltage) > theParameter )
			|| ( ! theConnections.validMinGateLeak && theConnections.validMaxSourceLeak
				&& abs(theConnections.maxSourceLeakVoltage) > theParameter )
			|| ( ! theConnections.validMinGateLeak && theConnections.validMaxDrainLeak
				&& abs(theConnections.maxDrainLeakVoltage) > theParameter )
			|| ( ! theConnections.validMaxGateLeak && theConnections.validMinSourceLeak
				&& abs(theConnections.minSourceLeakVoltage) > theParameter )
			|| ( ! theConnections.validMaxGateLeak && theConnections.validMinDrainLeak
				&& abs(theConnections.minDrainLeakVoltage) > theParameter )	) {
		return("Overvoltage Error:Gate vs Source/Drain: (logic ok)");
	}
	return("");
}

void CCvcDb::FindOverVoltageErrors(string theCheck, int theErrorIndex) {
	CFullConnection myConnections;
	reportFile << "! Checking " << theCheck << " overvoltage errors" << endl << endl;
	errorFile << "! Checking " << theCheck << " overvoltage errors" << endl << endl;
	for (CModelListMap::iterator keyModelListPair_pit = cvcParameters.cvcModelListMap.begin(); keyModelListPair_pit != cvcParameters.cvcModelListMap.end(); keyModelListPair_pit++) {
		for (CModelList::iterator model_pit = keyModelListPair_pit->second.begin(); model_pit != keyModelListPair_pit->second.end(); model_pit++) {
			if ( theErrorIndex == OVERVOLTAGE_VBG && model_pit->maxVbg == UNKNOWN_VOLTAGE) continue;
			if ( theErrorIndex == OVERVOLTAGE_VBS && model_pit->maxVbs == UNKNOWN_VOLTAGE) continue;
			if ( theErrorIndex == OVERVOLTAGE_VDS && model_pit->maxVds == UNKNOWN_VOLTAGE) continue;
			if ( theErrorIndex == OVERVOLTAGE_VGS && model_pit->maxVgs == UNKNOWN_VOLTAGE) continue;
			CDevice * myDevice_p = model_pit->firstDevice_p;
			string myDisplayParameter;
			if ( theErrorIndex == OVERVOLTAGE_VBG ) {
				myDisplayParameter = " Vbg=" + PrintToleranceParameter(model_pit->maxVbgDefinition, model_pit->maxVbg, VOLTAGE_SCALE);
			} else if ( theErrorIndex == OVERVOLTAGE_VBS ) {
				myDisplayParameter = " Vbs=" + PrintToleranceParameter(model_pit->maxVbsDefinition, model_pit->maxVbs, VOLTAGE_SCALE);
			} else if ( theErrorIndex == OVERVOLTAGE_VDS ) {
				myDisplayParameter = " Vds=" + PrintToleranceParameter(model_pit->maxVdsDefinition, model_pit->maxVds, VOLTAGE_SCALE);
			} else if ( theErrorIndex == OVERVOLTAGE_VGS ) {
				myDisplayParameter = " Vgs=" + PrintToleranceParameter(model_pit->maxVgsDefinition, model_pit->maxVgs, VOLTAGE_SCALE);
			}
			myDisplayParameter += " " + model_pit->ConditionString();
			while (myDevice_p) {
				CCircuit * myParent_p = myDevice_p->parent_p;
				for (instanceId_t instance_it = 0; instance_it < myParent_p->instanceId_v.size(); instance_it++) {
					CInstance * myInstance_p = instancePtr_v[myParent_p->instanceId_v[instance_it]];
					MapDeviceNets(myInstance_p, myDevice_p, myConnections);
					myConnections.SetMinMaxLeakVoltagesAndFlags(this);
					string myErrorExplanation = "";
					if ( theErrorIndex == OVERVOLTAGE_VBG ) {
						myErrorExplanation = FindVbgError(model_pit->maxVbg, myConnections);
					} else if ( theErrorIndex == OVERVOLTAGE_VBS ) {
						myErrorExplanation = FindVbsError(model_pit->maxVbs, myConnections);
					} else if ( theErrorIndex == OVERVOLTAGE_VDS ) {
						myErrorExplanation = FindVdsError(model_pit->maxVds, myConnections);
					} else if ( theErrorIndex == OVERVOLTAGE_VGS ) {
						myErrorExplanation = FindVgsError(model_pit->maxVgs, myConnections);
					}
					if ( model_pit->maxVbg != UNKNOWN_VOLTAGE ) {
					}
					if ( ! IsEmpty(myErrorExplanation) ) {
						errorCount[theErrorIndex]++;
						if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
							errorFile << myErrorExplanation + myDisplayParameter << endl;;
							bool myLeakCheckFlag = ( myErrorExplanation.find("logic ok") < string::npos );
							PrintDeviceWithMinMaxConnections(myParent_p->instanceId_v[instance_it], myConnections, errorFile, myLeakCheckFlag);
							errorFile << endl;
						}
					}
				}
				myDevice_p = myDevice_p->nextDevice_p;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking " + theCheck + " overvoltage errors");
//	errorFile << "! Finished" << endl << endl;
}

//void CCvcDb::FindOverVoltageErrors() {
//	CFullConnection myConnections;
//	for (CModelListMap::iterator keyModelListPair_pit = cvcParameters.cvcModelListMap.begin(); keyModelListPair_pit != cvcParameters.cvcModelListMap.end(); keyModelListPair_pit++) {
//		for (CModelList::iterator model_pit = keyModelListPair_pit->second.begin(); model_pit != keyModelListPair_pit->second.end(); model_pit++) {
//			if ( model_pit->maxVbg == UNKNOWN_VOLTAGE && model_pit->maxVgs == UNKNOWN_VOLTAGE && model_pit->maxVbs == UNKNOWN_VOLTAGE && model_pit->maxVds == UNKNOWN_VOLTAGE ) {
//				if ( model_pit->type != SWITCH_OFF && model_pit->type != SWITCH_ON && model_pit->firstDevice_p ) {
//					reportFile << "WARNING: no overvoltage check for model: " << model_pit->definition << endl << endl;
//				}
//				continue;
//			}
//			reportFile << "! Checking overvoltage errors for model: " << model_pit->definition << endl << endl;
//			errorFile << "! Checking overvoltage errors for model: " << model_pit->definition << endl << endl;
//			CDevice * myDevice_p = model_pit->firstDevice_p;
//			while (myDevice_p) {
//				CCircuit * myParent_p = myDevice_p->parent_p;
////				deviceId_t myLocalDeviceId = myParent_p->localDeviceIdMap[myDevice_p->name];
//				for (instanceId_t instance_it = 0; instance_it < myParent_p->instanceId_v.size(); instance_it++) {
//					CInstance * myInstance_p = instancePtr_v[myParent_p->instanceId_v[instance_it]];
//					MapDeviceNets(myInstance_p, myDevice_p, myConnections);
////					myConnections.SetUnknownVoltage();
//					myConnections.SetMinMaxLeakVoltages(this);
//					bool myErrorFlag = false;
//					bool myLeakCheckFlag = false;
//					string myErrorExplanation;
//					if ( model_pit->maxVbg != UNKNOWN_VOLTAGE ) {
//						if ( ( myConnections.CheckTerminalMinMaxVoltages(GATE | BULK, false) &&
//									( abs(myConnections.maxGateVoltage - myConnections.minBulkVoltage) > model_pit->maxVbg ||
//											abs(myConnections.minGateVoltage - myConnections.maxBulkVoltage) > model_pit->maxVbg ) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(GATE, false) && ! myConnections.CheckTerminalMinMaxVoltages(BULK, false) &&
//										max(abs(myConnections.minGateVoltage), abs(myConnections.maxGateVoltage)) > model_pit->maxVbg ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(GATE, false) && myConnections.CheckTerminalMinMaxVoltages(BULK, false) &&
//										max(abs(myConnections.minBulkVoltage), abs(myConnections.maxBulkVoltage)) > model_pit->maxVbg ) ) {
//							myErrorExplanation += "Overvoltage Error:Gate vs Bulk:\n";
//							myErrorFlag = true;
//						} else if ( ( myConnections.CheckTerminalMinMaxLeakVoltages(GATE | BULK) &&
//									( abs(myConnections.maxGateLeakVoltage - myConnections.minBulkLeakVoltage) > model_pit->maxVbg ||
//											abs(myConnections.minGateLeakVoltage - myConnections.maxBulkLeakVoltage) > model_pit->maxVbg ) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(GATE) && ! myConnections.CheckTerminalMinMaxLeakVoltages(BULK) &&
//										max(abs(myConnections.minGateLeakVoltage), abs(myConnections.maxGateLeakVoltage)) > model_pit->maxVbg ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(GATE) && myConnections.CheckTerminalMinMaxLeakVoltages(BULK) &&
//										max(abs(myConnections.minBulkLeakVoltage), abs(myConnections.maxBulkLeakVoltage)) > model_pit->maxVbg ) ) {
//							myErrorExplanation += "Overvoltage Error:Gate vs Bulk: (logic ok)\n";
//							myErrorFlag = true;
//							myLeakCheckFlag = true;
//						}
//					}
//					if ( model_pit->maxVgs != UNKNOWN_VOLTAGE ) {
//						if ( ( myConnections.CheckTerminalMinMaxVoltages(GATE | SOURCE, false) &&
//									( abs(myConnections.maxGateVoltage - myConnections.minSourceVoltage) > model_pit->maxVgs ||
//										abs(myConnections.minGateVoltage - myConnections.maxSourceVoltage) > model_pit->maxVgs ) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(GATE | DRAIN, false) &&
//									( abs(myConnections.maxGateVoltage - myConnections.minDrainVoltage) > model_pit->maxVgs ||
//										abs(myConnections.minGateVoltage - myConnections.maxDrainVoltage) > model_pit->maxVgs ) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(GATE, false) && ! myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN, false) &&
//										max(abs(myConnections.minGateVoltage), abs(myConnections.maxGateVoltage)) > model_pit->maxVgs ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(GATE, false) && myConnections.CheckTerminalMinMaxVoltages(SOURCE, false) &&
//										max(abs(myConnections.minSourceVoltage), abs(myConnections.maxSourceVoltage)) > model_pit->maxVgs ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(GATE, false) && myConnections.CheckTerminalMinMaxVoltages(DRAIN, false) &&
//										max(abs(myConnections.minDrainVoltage), abs(myConnections.maxDrainVoltage)) > model_pit->maxVgs ) ) {
//							myErrorExplanation += "Overvoltage Error:Gate vs Source/Drain:\n";
//							myErrorFlag = true;
//						} else if ( ( myConnections.CheckTerminalMinMaxLeakVoltages(GATE | SOURCE) &&
//									( abs(myConnections.maxGateLeakVoltage - myConnections.minSourceLeakVoltage) > model_pit->maxVgs ||
//										abs(myConnections.minGateLeakVoltage - myConnections.maxSourceLeakVoltage) > model_pit->maxVgs ) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(GATE | DRAIN) &&
//									( abs(myConnections.maxGateLeakVoltage - myConnections.minDrainLeakVoltage) > model_pit->maxVgs ||
//										abs(myConnections.minGateLeakVoltage - myConnections.maxDrainLeakVoltage) > model_pit->maxVgs ) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(GATE) && ! myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE | DRAIN) &&
//										max(abs(myConnections.minGateLeakVoltage), abs(myConnections.maxGateLeakVoltage)) > model_pit->maxVgs ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(GATE) && myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE) &&
//										max(abs(myConnections.minSourceLeakVoltage), abs(myConnections.maxSourceLeakVoltage)) > model_pit->maxVgs ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(GATE) && myConnections.CheckTerminalMinMaxLeakVoltages(DRAIN) &&
//										max(abs(myConnections.minDrainLeakVoltage), abs(myConnections.maxDrainLeakVoltage)) > model_pit->maxVgs ) ) {
//							myErrorExplanation += "Overvoltage Error:Gate vs Source/Drain: (logic ok)\n";
//							myErrorFlag = true;
//							myLeakCheckFlag = true;
//						}
//					}
//					if ( model_pit->maxVbs != UNKNOWN_VOLTAGE ) {
//						if ( ( myConnections.CheckTerminalMinMaxVoltages(SOURCE | BULK, false) &&
//									(abs(myConnections.maxBulkVoltage - myConnections.minSourceVoltage) > model_pit->maxVbs ||
//										abs(myConnections.minBulkVoltage - myConnections.maxSourceVoltage) > model_pit->maxVbs) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(DRAIN | BULK, false) &&
//									(abs(myConnections.maxBulkVoltage - myConnections.minDrainVoltage) > model_pit->maxVbs ||
//										abs(myConnections.minBulkVoltage - myConnections.maxDrainVoltage) > model_pit->maxVbs ) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(BULK, false) && ! myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN, false) &&
//									max(abs(myConnections.minBulkVoltage), abs(myConnections.maxBulkVoltage)) > model_pit->maxVbs ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(BULK, false) && myConnections.CheckTerminalMinMaxVoltages(SOURCE, false) &&
//									max(abs(myConnections.minSourceVoltage), abs(myConnections.maxSourceVoltage)) > model_pit->maxVbs ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(BULK, false) && myConnections.CheckTerminalMinMaxVoltages(DRAIN, false) &&
//									max(abs(myConnections.minDrainVoltage), abs(myConnections.maxDrainVoltage)) > model_pit->maxVbs ) )	{
//							myErrorExplanation += "Overvoltage Error:Source/Drain vs Bulk:\n";
//							myErrorFlag = true;
//						} else if ( ( myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE | BULK) &&
//									(abs(myConnections.maxBulkLeakVoltage - myConnections.minSourceLeakVoltage) > model_pit->maxVbs ||
//										abs(myConnections.minBulkLeakVoltage - myConnections.maxSourceLeakVoltage) > model_pit->maxVbs) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(DRAIN | BULK) &&
//									(abs(myConnections.maxBulkLeakVoltage - myConnections.minDrainLeakVoltage) > model_pit->maxVbs ||
//										abs(myConnections.minBulkLeakVoltage - myConnections.maxDrainLeakVoltage) > model_pit->maxVbs ) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(BULK) && ! myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE | DRAIN) &&
//									max(abs(myConnections.minBulkLeakVoltage), abs(myConnections.maxBulkLeakVoltage)) > model_pit->maxVbs ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(BULK) && myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE) &&
//									max(abs(myConnections.minSourceLeakVoltage), abs(myConnections.maxSourceLeakVoltage)) > model_pit->maxVbs ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(BULK) && myConnections.CheckTerminalMinMaxLeakVoltages(DRAIN) &&
//									max(abs(myConnections.minDrainLeakVoltage), abs(myConnections.maxDrainLeakVoltage)) > model_pit->maxVbs ) )	{
//							myErrorExplanation += "Overvoltage Error:Source/Drain vs Bulk: (logic ok)\n";
//							myErrorFlag = true;
//							myLeakCheckFlag = true;
//						}
//					}
//					if ( model_pit->maxVds != UNKNOWN_VOLTAGE ) {
//						if ( ( myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN, false) &&
//									(abs(myConnections.maxSourceVoltage - myConnections.minDrainVoltage) > model_pit->maxVds ||
//										abs(myConnections.minSourceVoltage - myConnections.maxDrainVoltage) > model_pit->maxVds ) ) ||
//								( myConnections.CheckTerminalMinMaxVoltages(SOURCE, false) && ! myConnections.CheckTerminalMinMaxVoltages(DRAIN, false) &&
//									max(abs(myConnections.minSourceVoltage), abs(myConnections.maxSourceVoltage)) > model_pit->maxVds ) ||
//								( ! myConnections.CheckTerminalMinMaxVoltages(SOURCE, false) && myConnections.CheckTerminalMinMaxVoltages(DRAIN, false) &&
//									max(abs(myConnections.minDrainVoltage), abs(myConnections.maxDrainVoltage)) > model_pit->maxVds ) ) {
//							if ( myConnections.IsPumpCapacitor() &&
//									abs(myConnections.minSourceVoltage - myConnections.minDrainVoltage) <= model_pit->maxVds &&
//									abs(myConnections.maxSourceVoltage - myConnections.maxDrainVoltage)	<= model_pit->maxVds ) {
//								; // for pumping capacitors only check min-min/max-max differences
//							} else {
//								myErrorExplanation += "Overvoltage Error:Source vs Drain:\n";
//								myErrorFlag = true;
//							}
//						} else if ( ( myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE | DRAIN) &&
//									(abs(myConnections.maxSourceLeakVoltage - myConnections.minDrainLeakVoltage) > model_pit->maxVds ||
//										abs(myConnections.minSourceLeakVoltage - myConnections.maxDrainLeakVoltage) > model_pit->maxVds ) ) ||
//								( myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE) && ! myConnections.CheckTerminalMinMaxLeakVoltages(DRAIN) &&
//									max(abs(myConnections.minSourceLeakVoltage), abs(myConnections.maxSourceLeakVoltage)) > model_pit->maxVds ) ||
//								( ! myConnections.CheckTerminalMinMaxLeakVoltages(SOURCE) && myConnections.CheckTerminalMinMaxLeakVoltages(DRAIN) &&
//									max(abs(myConnections.minDrainLeakVoltage), abs(myConnections.maxDrainLeakVoltage)) > model_pit->maxVds ) ) {
//							myErrorExplanation += "Overvoltage Error:Source vs Drain: (logic ok)\n";
//							myErrorFlag = true;
//							myLeakCheckFlag = true;
//						}
//					}
//					if ( myErrorFlag ) {
//						errorCount[OVERVOLTAGE]++;
//						if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
//							errorFile << myErrorExplanation;
//							PrintDeviceWithMinMaxConnections(myParent_p->instanceId_v[instance_it], myConnections, errorFile, myLeakCheckFlag);
//							errorFile << endl;
//						}
//					}
//				}
//				myDevice_p = myDevice_p->nextDevice_p;
//			}
//		}
//	}
//	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile);
//	errorFile << "! Finished" << endl << endl;
//}

void CCvcDb::FindNmosGateVsSourceErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking nmos gate vs source errors: " << endl << endl;
	errorFile << "! Checking nmos gate vs source errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsNmos_(myDevice_p->model_p->type) ) continue;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		bool myVthFlag = false;
		bool myUnrelatedFlag = false;
		if ( ! myConnections.minGatePower_p ) continue;
		if ( myConnections.minGatePower_p->IsRelative(myConnections.minSourcePower_p, true, true)
				&& myConnections.minGatePower_p->IsRelative(myConnections.minDrainPower_p, true, true) ) {
			// if relatives (default), then checks are conditional
			voltage_t myGateSourceDifference = myConnections.minGateVoltage - min(myConnections.minSourceVoltage, myConnections.minSourceVoltage + myDevice_p->model_p->Vth);
			voltage_t myGateDrainDifference = myConnections.minGateVoltage - min(myConnections.minDrainVoltage, myConnections.minDrainVoltage + myDevice_p->model_p->Vth);
			voltage_t myMaxVoltageDifference = min(0, myDevice_p->model_p->Vth);
			if ( myConnections.CheckTerminalMinVoltages(GATE | SOURCE)
					&& myGateSourceDifference > myMaxVoltageDifference
					&& myConnections.gateId != myConnections.drainId ) {
				myMaxVoltageDifference = myGateSourceDifference;
			}
			if ( myConnections.CheckTerminalMinVoltages(GATE | DRAIN)
					&& myGateDrainDifference > myMaxVoltageDifference
					&& myConnections.gateId != myConnections.sourceId ) {
				myMaxVoltageDifference = myGateDrainDifference;
			}
			if ( myMaxVoltageDifference <= cvcParameters.cvcGateErrorThreshold
					|| ( cvcParameters.cvcMinVthGates && myMaxVoltageDifference < myDevice_p->model_p->Vth ) ) continue;  // no error
			// Skip gates that are always fully on
			if ( myConnections.CheckTerminalMaxVoltages(SOURCE) ) {
				if ( myConnections.CheckTerminalMaxVoltages(DRAIN) ) {
					if ( myConnections.minGateVoltage >= max(myConnections.maxSourceVoltage, myConnections.maxDrainVoltage) - cvcParameters.cvcGateErrorThreshold ) continue;
					if ( myConnections.sourceId == myConnections.drainId ) {  // capacitor check
						if ( IsPower_(netVoltagePtr_v[myConnections.gateId]) && IsPower_(netVoltagePtr_v[myConnections.drainId]) ) continue;  // ignore direct power capacitors
						if ( ! IsInputOrPower_(netVoltagePtr_v[myConnections.masterMinSourceNet.finalNetId])
								|| ! IsInputOrPower_(netVoltagePtr_v[myConnections.masterMinGateNet.finalNetId]) ) continue;  // ignore capacitors connected to non-input/power nets
					}
				} else {
					if ( myConnections.minGateVoltage >= myConnections.maxSourceVoltage - cvcParameters.cvcGateErrorThreshold ) continue;
				}
			} else if ( myConnections.CheckTerminalMaxVoltages(DRAIN) ) {
				if ( myConnections.minGateVoltage >= myConnections.maxDrainVoltage - cvcParameters.cvcGateErrorThreshold ) continue;
			} else {
				continue;  // ignore devices with no max connections
			}
			myVthFlag = myConnections.minGatePower_p->type[MIN_CALCULATED_BIT]
				&& ( myConnections.minGateVoltage - myConnections.minSourceVoltage == myDevice_p->model_p->Vth
					|| myConnections.minGateVoltage - myConnections.minDrainVoltage == myDevice_p->model_p->Vth );
			if ( myVthFlag && ! cvcParameters.cvcVthGates ) continue;
		} else {
			myUnrelatedFlag = true;  // if not relatives, always an error
		}
		errorCount[NMOS_GATE_SOURCE]++;
		if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
			if ( myUnrelatedFlag ) {
				errorFile << "Unrelated power error" << endl;
			} else if ( myConnections.minGatePower_p->type[REFERENCE_BIT] ) {
				errorFile << "Gate reference signal" << endl;
			} else if ( myVthFlag ) {
				errorFile << "Gate-source = Vth" << endl;
			}
			PrintDeviceWithMinConnections(deviceParent_v[device_it], myConnections, errorFile);
			errorFile << endl;
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking nmos gate vs source errors: ");
}

void CCvcDb::FindPmosGateVsSourceErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking pmos gate vs source errors: " << endl << endl;
	errorFile << "! Checking pmos gate vs source errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsPmos_(myDevice_p->model_p->type) ) continue;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		bool myVthFlag = false;
		bool myUnrelatedFlag = false;
		if ( ! myConnections.maxGatePower_p ) continue;
		if ( myConnections.maxGatePower_p->IsRelative(myConnections.maxSourcePower_p, true, true)
				&& myConnections.maxGatePower_p->IsRelative(myConnections.maxDrainPower_p, true, true) ) {
			// if relatives (default), then checks are conditional
			voltage_t myGateSourceDifference = max(myConnections.maxSourceVoltage, myConnections.maxSourceVoltage + myDevice_p->model_p->Vth) - myConnections.maxGateVoltage;
			voltage_t myGateDrainDifference = max(myConnections.maxDrainVoltage, myConnections.maxDrainVoltage + myDevice_p->model_p->Vth) - myConnections.maxGateVoltage;
			voltage_t myMaxVoltageDifference = max(0, myDevice_p->model_p->Vth);
			if ( myConnections.CheckTerminalMaxVoltages(GATE | SOURCE) &&
					myGateSourceDifference > myMaxVoltageDifference  &&
					myConnections.gateId != myConnections.drainId ) {
				myMaxVoltageDifference = myGateSourceDifference;
			}
			if ( myConnections.CheckTerminalMaxVoltages(GATE | DRAIN) &&
					myGateDrainDifference > myMaxVoltageDifference  &&
					myConnections.gateId != myConnections.sourceId ) {
				myMaxVoltageDifference = myGateDrainDifference;
			}
			if ( myMaxVoltageDifference <= cvcParameters.cvcGateErrorThreshold
					|| ( cvcParameters.cvcMinVthGates && myMaxVoltageDifference < -myDevice_p->model_p->Vth ) ) continue;  // no error
			// Skip gates that are always fully on
			if ( myConnections.CheckTerminalMinVoltages(SOURCE) ) {
				if ( myConnections.CheckTerminalMinVoltages(DRAIN) ) {
					if ( myConnections.maxGateVoltage <= min(myConnections.minSourceVoltage, myConnections.minDrainVoltage) + cvcParameters.cvcGateErrorThreshold ) continue;
					if ( myConnections.sourceId == myConnections.drainId ) {  // capacitor check
						if ( IsPower_(netVoltagePtr_v[myConnections.gateId]) && IsPower_(netVoltagePtr_v[myConnections.drainId]) ) continue;  // ignore direct power capacitors
						if ( ! IsInputOrPower_(netVoltagePtr_v[myConnections.masterMinSourceNet.finalNetId])
								|| ! IsInputOrPower_(netVoltagePtr_v[myConnections.masterMinGateNet.finalNetId]) ) continue;  // ignore capacitors connected to non-input/power nets
					}
				} else {
					if ( myConnections.maxGateVoltage <= myConnections.minSourceVoltage + cvcParameters.cvcGateErrorThreshold ) continue;
				}
			} else if ( myConnections.CheckTerminalMinVoltages(DRAIN) ) {
				if ( myConnections.maxGateVoltage <= myConnections.minDrainVoltage + cvcParameters.cvcGateErrorThreshold ) continue;
			} else {
				continue;  // ignore devices with no min connections
			}
			myVthFlag= myConnections.maxGatePower_p->type[MAX_CALCULATED_BIT]
				&& ( myConnections.maxGateVoltage - myConnections.maxSourceVoltage == myDevice_p->model_p->Vth
					|| myConnections.maxGateVoltage - myConnections.maxDrainVoltage == myDevice_p->model_p->Vth );
			if ( myVthFlag && ! cvcParameters.cvcVthGates ) continue;
		} else {
			myUnrelatedFlag = true; // if not relatives, always an error
		}
		errorCount[PMOS_GATE_SOURCE]++;
		if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
			if ( myUnrelatedFlag ) {
				errorFile << "Unrelated power error" << endl;
			} else if ( myConnections.maxGatePower_p->type[REFERENCE_BIT] ) {
				errorFile << "Gate reference signal" << endl;
			} else if ( myVthFlag ) {
				errorFile << "Gate-source = Vth" << endl;
			}
			PrintDeviceWithMaxConnections(deviceParent_v[device_it], myConnections, errorFile);
			errorFile << endl;
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking pmos gate vs source errors: ");
}

void CCvcDb::FindNmosSourceVsBulkErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking nmos source/drain vs bias errors: " << endl << endl;
	errorFile << "! Checking nmos source/drain vs bias errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsNmos_(myDevice_p->model_p->type) ) continue;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		if ( myConnections.sourceId == myConnections.drainId && myConnections.sourceId == myConnections.bulkId ) continue;  // ignore drain = source = bulk
		bool myErrorFlag = false;
		bool myUnrelatedFlag = false;
		if ( ! myConnections.minBulkPower_p
				|| (myConnections.minBulkPower_p->IsRelative(myConnections.minSourcePower_p, true, true)
						&& myConnections.minBulkPower_p->IsRelative(myConnections.minDrainPower_p, true, true)) ) {
			// if relatives (default), then checks are conditional
			if ( (myConnections.maxBulkVoltage == myConnections.minSourceVoltage || myConnections.minSourceVoltage == UNKNOWN_VOLTAGE)
					&& (myConnections.maxBulkVoltage == myConnections.minDrainVoltage || myConnections.minDrainVoltage == UNKNOWN_VOLTAGE) ) continue;
				// no error if max bulk = min source = min drain
			if ( myConnections.minBulkPower_p && myConnections.minBulkPower_p->type[HIZ_BIT] ) {
				if ( myConnections.sourceId == myConnections.bulkId && myConnections.drainId == myConnections.bulkId ) continue;
				if ( myConnections.minSourcePower_p && ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.minSourcePower_p, netVoltagePtr_v, minNet_v, minNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.simSourcePower_p && IsKnownVoltage_(myConnections.simSourcePower_p->simVoltage)
						&& ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.simSourcePower_p, netVoltagePtr_v, minNet_v, simNet_v, false)
						&& myConnections.simSourceVoltage < myConnections.maxBulkVoltage ) {
					myErrorFlag = true;
				} else if ( myConnections.maxSourcePower_p && ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.maxSourcePower_p, netVoltagePtr_v, minNet_v, maxNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.minDrainPower_p && ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.minDrainPower_p, netVoltagePtr_v, minNet_v, minNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.simDrainPower_p && IsKnownVoltage_(myConnections.simDrainPower_p->simVoltage)
						&& ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.simDrainPower_p, netVoltagePtr_v, minNet_v, simNet_v, false)
						&& myConnections.simDrainVoltage < myConnections.maxBulkVoltage ) {
					myErrorFlag = true;
				} else if ( myConnections.maxDrainPower_p && ! myConnections.minBulkPower_p->IsRelatedPower(myConnections.maxDrainPower_p, netVoltagePtr_v, minNet_v, maxNet_v, false) ) {
					myErrorFlag = true;
				}
			} else if ( myConnections.CheckTerminalMinVoltages(BULK) == false ) {
				if ( myConnections.minSourcePower_p || myConnections.simSourcePower_p || myConnections.maxSourcePower_p ||
						myConnections.minDrainPower_p || myConnections.simDrainPower_p || myConnections.maxDrainPower_p ) { // has some connection (all connections open -> no error)
					myErrorFlag = true;
				}
			} else if ( myConnections.CheckTerminalMinVoltages(BULK | SOURCE) &&
					( myConnections.minBulkVoltage - myConnections.minSourceVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.minSourceVoltage == myConnections.minBulkVoltage &&
							myConnections.masterMinSourceNet.finalResistance < myConnections.masterMinBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalMinVoltages(BULK | DRAIN) &&
					( myConnections.minBulkVoltage - myConnections.minDrainVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.minDrainVoltage == myConnections.minBulkVoltage &&
							myConnections.masterMinDrainNet.finalResistance < myConnections.masterMinBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalSimVoltages(BULK | SOURCE) &&
					( myConnections.simBulkVoltage - myConnections.simSourceVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.simSourceVoltage == myConnections.simBulkVoltage &&
							myConnections.masterSimSourceNet.finalResistance < myConnections.masterSimBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalSimVoltages(BULK | DRAIN) &&
					( myConnections.simBulkVoltage - myConnections.simDrainVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.simDrainVoltage == myConnections.simBulkVoltage &&
							myConnections.masterSimDrainNet.finalResistance < myConnections.masterSimBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalMaxVoltages(BULK | SOURCE) &&
					( ( myConnections.maxBulkVoltage - myConnections.maxSourceVoltage > cvcParameters.cvcBiasErrorThreshold &&
							myConnections.maxSourcePower_p->defaultMaxNet != myConnections.bulkId ) ||
						( myConnections.maxSourceVoltage == myConnections.maxBulkVoltage &&
							myConnections.maxSourceVoltage != myConnections.minBulkVoltage && // no leak path
	//						myConnections.masterMinBulkNet.finalNetId != myConnections.masterMaxBulkNet.finalNetId &&
							myConnections.masterMaxSourceNet.finalResistance > myConnections.masterMaxBulkNet.finalResistance &&
							! myConnections.minBulkPower_p->type[HIZ_BIT] &&
							! PathContains(maxNet_v, myConnections.sourceId, myConnections.bulkId)) ) ) { // resistance check backwards in NMOS max (ignore connections through self)
				myErrorFlag = true;
	// Added 20140523
			} else if (	myConnections.CheckTerminalMaxVoltages(BULK | DRAIN) &&
					( ( myConnections.maxBulkVoltage - myConnections.maxDrainVoltage > cvcParameters.cvcBiasErrorThreshold &&
							myConnections.maxDrainPower_p->defaultMaxNet != myConnections.bulkId ) ||
						( myConnections.maxDrainVoltage == myConnections.maxBulkVoltage &&
							myConnections.maxDrainVoltage != myConnections.minBulkVoltage && // no leak path
	//						myConnections.masterMinBulkNet.finalNetId != myConnections.masterMaxBulkNet.finalNetId &&
							myConnections.masterMaxDrainNet.finalResistance > myConnections.masterMaxBulkNet.finalResistance &&
							! myConnections.minBulkPower_p->type[HIZ_BIT] &&
							! PathContains(maxNet_v, myConnections.drainId, myConnections.bulkId)) ) ) { // resistance check backwards in NMOS max (ignore connections through self)
				myErrorFlag = true;
			}
		} else {
			myErrorFlag = true; // if not relatives, always an error
			myUnrelatedFlag = true;
		}
		if ( myErrorFlag ) {
			errorCount[NMOS_SOURCE_BULK]++;
			if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
				if ( myUnrelatedFlag ) {
					errorFile << "Unrelated power error" << endl;
				}
				PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
				errorFile << endl;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking nmos source/drain vs bias errors: ");
}

void CCvcDb::FindPmosSourceVsBulkErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking pmos source/drain vs bias errors: " << endl << endl;
	errorFile << "! Checking pmos source/drain vs bias errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsPmos_(myDevice_p->model_p->type) ) continue;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		if ( myConnections.sourceId == myConnections.drainId && myConnections.sourceId == myConnections.bulkId ) continue;  // ignore drain = source = bulk
		bool myErrorFlag = false;
		bool myUnrelatedFlag = false;
		if ( ! myConnections.maxBulkPower_p
				|| (myConnections.maxBulkPower_p->IsRelative(myConnections.maxSourcePower_p, true, true)
						&& myConnections.maxBulkPower_p->IsRelative(myConnections.maxDrainPower_p, true, true)) ) {
			// if relatives (default), then checks are conditional
			if ( (myConnections.minBulkVoltage == myConnections.maxSourceVoltage || myConnections.maxSourceVoltage == UNKNOWN_VOLTAGE)
					&& (myConnections.minBulkVoltage == myConnections.maxDrainVoltage || myConnections.maxDrainVoltage == UNKNOWN_VOLTAGE) ) continue;
				 // no error if min bulk = max source = max drain
			if ( myConnections.maxBulkPower_p && myConnections.maxBulkPower_p->type[HIZ_BIT] ) {
				if ( myConnections.minSourcePower_p && ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.minSourcePower_p, netVoltagePtr_v, maxNet_v, minNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.simSourcePower_p && IsKnownVoltage_(myConnections.simSourcePower_p->simVoltage)
						&& ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.simSourcePower_p, netVoltagePtr_v, maxNet_v, simNet_v, false)
						&& myConnections.simSourceVoltage > myConnections.minBulkVoltage ) {
					myErrorFlag = true;
				} else if ( myConnections.maxSourcePower_p && ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.maxSourcePower_p, netVoltagePtr_v, maxNet_v, maxNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.minDrainPower_p && ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.minDrainPower_p, netVoltagePtr_v, maxNet_v, minNet_v, false) ) {
					myErrorFlag = true;
				} else if ( myConnections.simDrainPower_p && IsKnownVoltage_(myConnections.simDrainPower_p->simVoltage)
						&& ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.simDrainPower_p, netVoltagePtr_v, maxNet_v, simNet_v, false)
						&& myConnections.simDrainVoltage > myConnections.minBulkVoltage ) {
					myErrorFlag = true;
				} else if ( myConnections.maxDrainPower_p && ! myConnections.maxBulkPower_p->IsRelatedPower(myConnections.maxDrainPower_p, netVoltagePtr_v, maxNet_v, maxNet_v, false) ) {
					myErrorFlag = true;
				}
			} else if ( myConnections.CheckTerminalMaxVoltages(BULK) == false ) {
				if ( myConnections.minSourcePower_p || myConnections.simSourcePower_p || myConnections.maxSourcePower_p ||
						myConnections.minDrainPower_p || myConnections.simDrainPower_p || myConnections.maxDrainPower_p ) { // has some connection (all connections open -> no error)
					myErrorFlag = true;
				}
			} else if (	myConnections.CheckTerminalMinVoltages(BULK | SOURCE) &&
					( ( myConnections.minSourceVoltage - myConnections.minBulkVoltage > cvcParameters.cvcBiasErrorThreshold &&
							myConnections.minSourcePower_p->defaultMinNet != myConnections.bulkId ) ||
						( myConnections.minSourceVoltage == myConnections.minBulkVoltage &&
	//						myConnections.masterMinBulkNet.finalNetId != myConnections.masterMaxBulkNet.finalNetId &&
							myConnections.minSourceVoltage != myConnections.maxBulkVoltage && // no leak path
							myConnections.masterMinSourceNet.finalResistance > myConnections.masterMinBulkNet.finalResistance &&
							! myConnections.maxBulkPower_p->type[HIZ_BIT] &&
							! PathContains(minNet_v, myConnections.sourceId, myConnections.bulkId)) ) ) { // resistance check backwards in PMOS min (ignore connections through self)
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalMinVoltages(BULK | DRAIN) &&
					( ( myConnections.minDrainVoltage - myConnections.minBulkVoltage > cvcParameters.cvcBiasErrorThreshold &&
							myConnections.minDrainPower_p->defaultMinNet != myConnections.bulkId ) ||
						( myConnections.minDrainVoltage == myConnections.minBulkVoltage &&
	//						myConnections.masterMinBulkNet.finalNetId != myConnections.masterMaxBulkNet.finalNetId &&
							myConnections.minDrainVoltage != myConnections.maxBulkVoltage && // no leak path
							myConnections.masterMinDrainNet.finalResistance > myConnections.masterMinBulkNet.finalResistance &&
							! myConnections.maxBulkPower_p->type[HIZ_BIT] &&
							! PathContains(minNet_v, myConnections.drainId, myConnections.bulkId)) ) ) { // resistance check backwards in PMOS min (ignore connections through self)
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalSimVoltages(BULK | SOURCE) &&
					( myConnections.simSourceVoltage - myConnections.simBulkVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.simSourceVoltage == myConnections.simBulkVoltage &&
							myConnections.masterSimSourceNet.finalResistance < myConnections.masterSimBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalSimVoltages(BULK | DRAIN) &&
					( myConnections.simDrainVoltage - myConnections.simBulkVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.simDrainVoltage == myConnections.simBulkVoltage &&
							myConnections.masterSimDrainNet.finalResistance < myConnections.masterSimBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalMaxVoltages(BULK | SOURCE) &&
					( myConnections.maxSourceVoltage - myConnections.maxBulkVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.maxSourceVoltage == myConnections.maxBulkVoltage &&
							myConnections.masterMaxSourceNet.finalResistance < myConnections.masterMaxBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			} else if (	myConnections.CheckTerminalMaxVoltages(BULK | DRAIN) &&
					( myConnections.maxDrainVoltage - myConnections.maxBulkVoltage > cvcParameters.cvcBiasErrorThreshold ||
						( myConnections.maxDrainVoltage == myConnections.maxBulkVoltage &&
							myConnections.masterMaxDrainNet.finalResistance < myConnections.masterMaxBulkNet.finalResistance) ) ) {
				myErrorFlag = true;
			}
		} else {
			myUnrelatedFlag = true;
			myErrorFlag = true; // if not relatives, always an error
		}
		if ( myErrorFlag ) {
			errorCount[PMOS_SOURCE_BULK]++;
			if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
				if ( myUnrelatedFlag ) {
					errorFile << "Unrelated power error" << endl;
				}
				PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
				errorFile << endl;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking pmos source/drain vs bias errors: ");
}

void CCvcDb::FindForwardBiasDiodes() {
	CFullConnection myConnections;
	reportFile << "! Checking forward bias diode errors: " << endl << endl;
	errorFile << "! Checking forward bias diode errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( myDevice_p->model_p->type != DIODE ) continue;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		if ( GetEquivalentNet(myConnections.originalSourceId) == GetEquivalentNet(myConnections.originalDrainId) ) continue;
		bool myErrorFlag = false;
		bool myUnrelatedFlag = false;
		if ( myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN) ) {
			voltage_t mySourceVoltage = UNKNOWN_VOLTAGE, myDrainVoltage = UNKNOWN_VOLTAGE;
			if ( myConnections.minSourcePower_p == myConnections.minDrainPower_p ) {
				if ( myConnections.maxSourceVoltage == myConnections.maxDrainVoltage && myConnections.minSourceVoltage == myConnections.maxDrainVoltage ) continue;
				if ( myConnections.masterMinSourceNet.finalResistance < myConnections.masterMinDrainNet.finalResistance ) {
					mySourceVoltage = myConnections.minSourceVoltage;
				} else if ( myConnections.masterMinSourceNet.finalResistance > myConnections.masterMinDrainNet.finalResistance ) {
					myDrainVoltage = myConnections.minDrainVoltage;
				}
			}
			if ( myConnections.maxSourcePower_p == myConnections.maxDrainPower_p ) {
				if ( myConnections.masterMaxDrainNet.finalResistance < myConnections.masterMaxSourceNet.finalResistance ) {
					if (myDrainVoltage == UNKNOWN_VOLTAGE) {
						myDrainVoltage = myConnections.maxDrainVoltage; // min drain overrides
					} else if ( PathCrosses(maxNet_v, myConnections.sourceId, minNet_v, myConnections.drainId) ) {
						continue; // no error if anode to power crosses cathode to ground path
					} else {
						logFile << "INFO: unexpected diode " << DeviceName(device_it, PRINT_CIRCUIT_ON) << endl << endl;
						PrintVirtualNet<CVirtualNetVector>(maxNet_v, myConnections.sourceId, "Max anode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(minNet_v, myConnections.sourceId, "Min anode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(maxNet_v, myConnections.drainId, "Max cathode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(minNet_v, myConnections.drainId, "Min cathode path", logFile);
					}
				} else {
					if ( PathCrosses(minNet_v, myConnections.drainId, maxNet_v, myConnections.sourceId) ) {
						continue; // no error if anode to power crosses cathode to ground path
					} else if ( mySourceVoltage != UNKNOWN_VOLTAGE ) {
						logFile << "INFO: unexpected diode " << DeviceName(device_it, PRINT_CIRCUIT_ON) << endl << endl;
						PrintVirtualNet<CVirtualNetVector>(maxNet_v, myConnections.sourceId, "Max anode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(minNet_v, myConnections.sourceId, "Min anode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(maxNet_v, myConnections.drainId, "Max cathode path", logFile);
						PrintVirtualNet<CVirtualNetVector>(minNet_v, myConnections.drainId, "Min cathode path", logFile);
					}
					mySourceVoltage = myConnections.maxSourceVoltage; // max source overrides
				}
			}
			if ( mySourceVoltage == UNKNOWN_VOLTAGE ) {
				if ( cvcParameters.cvcLogicDiodes && myConnections.simSourceVoltage != UNKNOWN_VOLTAGE ) {
					mySourceVoltage = myConnections.simSourceVoltage;
				} else {
					mySourceVoltage = myConnections.maxSourceVoltage;
				}
			}
			if ( myDrainVoltage == UNKNOWN_VOLTAGE ) {
				if ( cvcParameters.cvcLogicDiodes && myConnections.simDrainVoltage != UNKNOWN_VOLTAGE ) {
					myDrainVoltage = myConnections.simDrainVoltage;
				} else {
					myDrainVoltage = myConnections.minDrainVoltage;
				}
			}
			if ( mySourceVoltage - myDrainVoltage > cvcParameters.cvcForwardErrorThreshold ) {
				if ( myConnections.maxSourcePower_p->type[HIZ_BIT] || myConnections.minDrainPower_p->type[HIZ_BIT] ) {
					// the following are errors for cutoff power. unknown annode, unknown cathode, source > max drain, max source not related to min drain
					if ( mySourceVoltage == UNKNOWN_VOLTAGE || myConnections.maxDrainVoltage == UNKNOWN_VOLTAGE || mySourceVoltage > myConnections.maxDrainVoltage ) {
							myErrorFlag = true;
					} else if ( ! myConnections.maxSourcePower_p->IsRelatedPower(myConnections.minDrainPower_p, netVoltagePtr_v, maxNet_v, minNet_v, true)) {
							myErrorFlag = true;
					}
				} else {
					myErrorFlag = true;
				}
			} else if ( ! myConnections.minSourcePower_p->IsRelative(myConnections.minDrainPower_p, true, true)
					|| ! myConnections.maxSourcePower_p->IsRelative(myConnections.maxDrainPower_p, true, true) ) {
				myErrorFlag = true;
				myUnrelatedFlag = true;
			}
		} else if ( myConnections.CheckTerminalMinMaxVoltages(SOURCE) && ! myConnections.CheckTerminalMinMaxVoltages(DRAIN) ) {
			if ( myConnections.maxSourceVoltage > 0 && ! myConnections.maxSourcePower_p->type[HIZ_BIT] ) {
				myErrorFlag = true;
			}
		} else if ( ! myConnections.CheckTerminalMinMaxVoltages(SOURCE) && myConnections.CheckTerminalMinMaxVoltages(DRAIN) ) {
			if ( myConnections.minDrainVoltage <= 0 && ! myConnections.minDrainPower_p->type[HIZ_BIT] ) {
				myErrorFlag = true;
			}
		}
		if ( myErrorFlag ) {
			errorCount[FORWARD_DIODE]++;
			if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
				if ( myUnrelatedFlag ) {
					errorFile << "Unrelated power error" << endl;
				}
				PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
				errorFile << endl;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking forward bias diode errors: ");
}

void CCvcDb::FindNmosPossibleLeakErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking nmos possible leak errors: " << endl << endl;
	errorFile << "! Checking nmos possible leak errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsNmos_(myDevice_p->model_p->type) ) continue;
		bool myErrorFlag = false;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		if ( IsKnownVoltage_(myConnections.simGateVoltage) ) continue;
		if ( myConnections.CheckTerminalMinVoltages(SOURCE | DRAIN) == true ) {
			voltage_t myMaxLeakGateVoltage = MaxLeakVoltage(myConnections.gateId);
			if ( myMaxLeakGateVoltage != UNKNOWN_VOLTAGE
					&& myMaxLeakGateVoltage <= myConnections.minSourceVoltage + myConnections.device_p->model_p->Vth
					&& myMaxLeakGateVoltage <= myConnections.minDrainVoltage + myConnections.device_p->model_p->Vth ) {
				continue;  // always off
			}
		}
		if ( myConnections.CheckTerminalSimVoltages(SOURCE | DRAIN) == true ) {
			if ( myConnections.simSourceVoltage == myConnections.simDrainVoltage ) continue;
			if ( abs(myConnections.simSourceVoltage - myConnections.simDrainVoltage) <= cvcParameters.cvcLeakErrorThreshold ) continue;
			if ( myConnections.simSourcePower_p->type[HIZ_BIT] &&
					! myConnections.simSourcePower_p->IsRelatedPower(myConnections.simDrainPower_p, netVoltagePtr_v, simNet_v, simNet_v, false) ) {
				;  // possible leaks to cutoff power
			} else if ( myConnections.simDrainPower_p->type[HIZ_BIT] &&
					! myConnections.simDrainPower_p->IsRelatedPower(myConnections.simSourcePower_p, netVoltagePtr_v, simNet_v, simNet_v, false) ) {
				;  // possible leaks to cutoff power
			} else if ( myConnections.simSourcePower_p->IsInternalOverride() || myConnections.simDrainPower_p->IsInternalOverride() ) {
				continue;
			} else if ( myConnections.simSourcePower_p->type[SIM_CALCULATED_BIT] || myConnections.simDrainPower_p->type[SIM_CALCULATED_BIT] ) {
				if ( myConnections.EstimatedCurrent() <= cvcParameters.cvcLeakLimit ) continue;
			}
			myErrorFlag = true;
		} else if ( myConnections.EstimatedMinimumCurrent() > cvcParameters.cvcLeakLimit ) {
			myErrorFlag = true;
		}
		if ( myErrorFlag ) {
			errorCount[NMOS_POSSIBLE_LEAK]++;
			if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
				PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
				errorFile << endl;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking nmos possible leak errors: ");
}

void CCvcDb::FindPmosPossibleLeakErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking pmos possible leak errors: " << endl << endl;
	errorFile << "! Checking pmos possible leak errors: " << endl << endl;
	for ( deviceId_t device_it = 0; device_it < deviceCount; device_it++ ) {
		CInstance * myInstance_p = instancePtr_v[deviceParent_v[device_it]];
		CCircuit * myParent_p = myInstance_p->master_p;
		CDevice * myDevice_p = myParent_p->devicePtr_v[device_it - myInstance_p->firstDeviceId];
		if ( ! IsPmos_(myDevice_p->model_p->type) ) continue;
		bool myErrorFlag = false;
		MapDeviceNets(myInstance_p, myDevice_p, myConnections);
		if ( IsKnownVoltage_(myConnections.simGateVoltage) ) continue;
		if ( myConnections.CheckTerminalMaxVoltages(SOURCE | DRAIN) == true ) {
			voltage_t myMinLeakGateVoltage = MinLeakVoltage(myConnections.gateId);
			if ( myMinLeakGateVoltage != UNKNOWN_VOLTAGE
					&& myMinLeakGateVoltage >= myConnections.maxSourceVoltage + myConnections.device_p->model_p->Vth
					&& myMinLeakGateVoltage >= myConnections.maxDrainVoltage + myConnections.device_p->model_p->Vth ) {
				continue;  // always off
			}
		}
		if ( myConnections.CheckTerminalSimVoltages(SOURCE | DRAIN) == true ) {
			if ( myConnections.simSourceVoltage == myConnections.simDrainVoltage ) continue;
			if ( abs(myConnections.simSourceVoltage - myConnections.simDrainVoltage) <= cvcParameters.cvcLeakErrorThreshold ) continue;
			if ( myConnections.simSourcePower_p->type[HIZ_BIT] &&
					! myConnections.simSourcePower_p->IsRelatedPower(myConnections.simDrainPower_p, netVoltagePtr_v, simNet_v, simNet_v, false) ) {
				;  // possible leaks to cutoff power
			} else if ( myConnections.simDrainPower_p->type[HIZ_BIT] &&
					! myConnections.simDrainPower_p->IsRelatedPower(myConnections.simSourcePower_p, netVoltagePtr_v, simNet_v, simNet_v, false) ) {
				;  // possible leaks to cutoff power
			} else if ( myConnections.simSourcePower_p->IsInternalOverride() || myConnections.simDrainPower_p->IsInternalOverride() ) {
				continue;
			} else if ( myConnections.simSourcePower_p->type[SIM_CALCULATED_BIT] || myConnections.simDrainPower_p->type[SIM_CALCULATED_BIT] ) {
				if ( myConnections.EstimatedCurrent() <= cvcParameters.cvcLeakLimit ) continue;
			}
			myErrorFlag = true;
		} else if ( myConnections.EstimatedMinimumCurrent() > cvcParameters.cvcLeakLimit ) {
			myErrorFlag = true;
		}
		if ( myErrorFlag ) {
			errorCount[PMOS_POSSIBLE_LEAK]++;
			if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
				PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
				errorFile << endl;
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking pmos possible leak errors: ");
}

void CCvcDb::FindFloatingInputErrors() {
	CFullConnection myConnections;
	reportFile << "! Checking mos floating input errors:" << endl << endl;
	errorFile << "! Checking mos floating input errors:" << endl << endl;
	bool myFloatingFlag;
	for (netId_t net_it = 0; net_it < netCount; net_it++) {
		if ( connectionCount_v[net_it].gateCount > 0 ) { // skips subordinate nets. only equivalent master nets have counts
			if ( firstGate_v[net_it] == UNKNOWN_DEVICE ) continue;
			MapDeviceNets(firstGate_v[net_it], myConnections);
			if ( myConnections.simGateVoltage != UNKNOWN_VOLTAGE ) continue;  // skip known voltages
			myFloatingFlag = IsFloatingGate(myConnections);
			if ( myFloatingFlag || myConnections.IsPossibleHiZ(this) ) {
				for ( deviceId_t device_it = firstGate_v[net_it]; device_it != UNKNOWN_DEVICE; device_it = nextGate_v[device_it] ) {
					MapDeviceNets(device_it, myConnections);
					bool myHasLeakPath = HasLeakPath(myConnections);
					if ( myFloatingFlag ) {
						deviceStatus_v[device_it][SIM_INACTIVE] = true;  // ignore true floating input gates (not possible floating)
					}
					if ( cvcParameters.cvcIgnoreVthFloating && IsAlwaysOff(myConnections) ) continue;  // skips Hi-Z input that is never on
					if ( myHasLeakPath || connectionCount_v[net_it].SourceDrainCount() == 0 ) { // physically floating gates too
//						CCircuit * myParent_p = instancePtr_v[deviceParent_v[device_it]]->master_p;
						errorCount[HIZ_INPUT]++;
						if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
							if ( ! myHasLeakPath ) errorFile << "* No leak path" << endl;
							if ( ! myFloatingFlag ) errorFile << "* Tri-state input" << endl;
							PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
							errorFile << endl;
						}
					}
				}
			}
		}
	}
	for (netId_t net_it = 0; net_it < netCount; net_it++) {  // second pass to catch floating nets caused by floating nets
		if ( connectionCount_v[net_it].gateCount > 0 ) { // skips subordinate nets. only equivalent master nets have counts
			if ( firstGate_v[net_it] == UNKNOWN_DEVICE ) continue;
			if ( SimVoltage(net_it) != UNKNOWN_VOLTAGE || (netVoltagePtr_v[net_it] && netVoltagePtr_v[net_it]->type[INPUT_BIT]) ) continue;
			if ( HasActiveConnections(net_it) ) continue;
			MapDeviceNets(firstGate_v[net_it], myConnections);
			if ( IsFloatingGate(myConnections) ) continue;  // Already processed previously
			for ( deviceId_t device_it = firstGate_v[net_it]; device_it != UNKNOWN_DEVICE; device_it = nextGate_v[device_it] ) {
				MapDeviceNets(device_it, myConnections);
				bool myHasLeakPath = HasLeakPath(myConnections);
				if ( myHasLeakPath ) {
	//						CCircuit * myParent_p = instancePtr_v[deviceParent_v[device_it]]->master_p;
					errorCount[HIZ_INPUT]++;
					if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
						errorFile << "* Secondary HI-Z error" << endl;
						PrintDeviceWithAllConnections(deviceParent_v[device_it], myConnections, errorFile);
						errorFile << endl;
					}
				}
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking mos floating input errors:");
//	errorFile << "! Finished" << endl << endl;
}

void CCvcDb::CheckExpectedValues() {
	CFullConnection myConnections;
	netId_t myNetId, mySimNetId, myMinNetId, myMaxNetId;
//	voltage_t mySimVoltage, myMinVoltage, myMaxVoltage;
	CVirtualNet mySimNet, myMinNet, myMaxNet;
	bool myExpectedValueFound;
	reportFile << "! Checking expected values:" << endl << endl;
	errorFile << "! Checking expected values:" << endl << endl;
	for (auto power_ppit = cvcParameters.cvcExpectedLevelPtrList.begin(); power_ppit != cvcParameters.cvcExpectedLevelPtrList.end(); power_ppit++) {
		size_t myLastErrorCount = errorCount[EXPECTED_VOLTAGE];
		myNetId = GetEquivalentNet((*power_ppit)->netId);
		if ( ! IsEmpty((*power_ppit)->expectedSim()) ) {
			mySimNet(simNet_v, myNetId);
			mySimNetId = mySimNet.finalNetId;
			myExpectedValueFound = false;
			myMinNet(minNet_v, myNetId);
			myMinNetId = myMinNet.finalNetId;
			myMaxNet(maxNet_v, myNetId);
			myMaxNetId = myMaxNet.finalNetId;
			if ( (*power_ppit)->expectedSim() == "open" ) {
				if ( ( mySimNetId == UNKNOWN_NET || ! netVoltagePtr_v[mySimNetId] || netVoltagePtr_v[mySimNetId]->simVoltage == UNKNOWN_VOLTAGE ) &&
						( myMinNetId == UNKNOWN_NET || ! netVoltagePtr_v[myMinNetId] || netVoltagePtr_v[myMinNetId]->minVoltage == UNKNOWN_VOLTAGE || netVoltagePtr_v[myMinNetId]->type[HIZ_BIT] ||
							myMaxNetId == UNKNOWN_NET || ! netVoltagePtr_v[myMaxNetId] || netVoltagePtr_v[myMaxNetId]->maxVoltage == UNKNOWN_VOLTAGE || netVoltagePtr_v[myMaxNetId]->type[HIZ_BIT]) ) { // open match
					myExpectedValueFound = true;
				}
			} else if ( mySimNetId != UNKNOWN_NET && netVoltagePtr_v[mySimNetId] && netVoltagePtr_v[mySimNetId]->simVoltage != UNKNOWN_VOLTAGE ) {
				if ( IsValidVoltage_((*power_ppit)->expectedSim())
						&& abs(String_to_Voltage((*power_ppit)->expectedSim()) - netVoltagePtr_v[mySimNetId]->simVoltage) <= cvcParameters.cvcExpectedErrorThreshold ) { // voltage match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedSim() == NetName(mySimNetId) ) { // name match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedSim() == string(netVoltagePtr_v[mySimNetId]->powerAlias()) ) { // alias match
					myExpectedValueFound = true;
				}
			}
			if ( ! myExpectedValueFound ) {
				errorCount[EXPECTED_VOLTAGE]++;
				errorFile << "Expected " << NetName(myNetId) << " = " << (*power_ppit)->expectedSim() << " but found ";
				if ( mySimNetId != UNKNOWN_NET && netVoltagePtr_v[mySimNetId] && netVoltagePtr_v[mySimNetId]->simVoltage != UNKNOWN_VOLTAGE ) {
					errorFile << NetName(mySimNetId) << "@" << PrintVoltage(netVoltagePtr_v[mySimNetId]->simVoltage) << endl;
				} else if ( (*power_ppit)->expectedSim() == "open" ) {
					bool myPrintedReason = false;
					if ( myMinNetId != UNKNOWN_NET && netVoltagePtr_v[myMinNetId] && netVoltagePtr_v[myMinNetId]->minVoltage != UNKNOWN_VOLTAGE ) {
						errorFile << "(min)" << NetName(myMinNetId) << "@" << PrintVoltage(netVoltagePtr_v[myMinNetId]->minVoltage) << " ";
						myPrintedReason = true;
					}
					if ( myMaxNetId != UNKNOWN_NET && netVoltagePtr_v[myMaxNetId] && netVoltagePtr_v[myMaxNetId]->maxVoltage != UNKNOWN_VOLTAGE ) {
						errorFile << "(max)" << NetName(myMaxNetId) << "@" << PrintVoltage(netVoltagePtr_v[myMaxNetId]->maxVoltage);
						myPrintedReason = true;
					}
					if ( myPrintedReason ) {
						errorFile << endl;
					} else {
						errorFile << "unknown" << endl;
					}
				} else {
					errorFile << "unknown" << endl;
				}
			}
		}
		if ( ! IsEmpty((*power_ppit)->expectedMin()) ) {
			myMinNet(minNet_v, myNetId);
			myMinNetId = myMinNet.finalNetId;
			myExpectedValueFound = false;
			if ( (*power_ppit)->expectedMin() == "open" ) {
				// simVoltage in the following condition is correct. open voltages can have min/max voltages.
				if ( myMinNetId == UNKNOWN_NET || ! netVoltagePtr_v[myMinNetId] || netVoltagePtr_v[myMinNetId]->simVoltage == UNKNOWN_VOLTAGE ) { // open match
					myExpectedValueFound = true;
				}
			// simVoltage in the following condition is correct. open voltages can have min/max voltages.
			} else if ( myMinNetId != UNKNOWN_NET && netVoltagePtr_v[myMinNetId] && netVoltagePtr_v[myMinNetId]->simVoltage != UNKNOWN_VOLTAGE ) {
				if ( String_to_Voltage((*power_ppit)->expectedMin()) == netVoltagePtr_v[myMinNetId]->minVoltage ) { // voltage match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedMin() == NetName(myMinNetId) ) { // name match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedMin() == string(netVoltagePtr_v[myMinNetId]->powerAlias()) ) { // alias match
					myExpectedValueFound = true;
				}
			}
			if ( ! myExpectedValueFound ) {
				errorCount[EXPECTED_VOLTAGE]++;
				errorFile << "Expected minimum " << NetName(myNetId) << " = " << (*power_ppit)->expectedMin() << " but found ";
				if ( myMinNetId != UNKNOWN_NET && netVoltagePtr_v[myMinNetId] && netVoltagePtr_v[myMinNetId]->minVoltage != UNKNOWN_VOLTAGE ) {
					errorFile << NetName(myMinNetId) << "@" << PrintVoltage(netVoltagePtr_v[myMinNetId]->minVoltage) << endl;
				} else {
					errorFile << "unknown" << endl;
				}
			}
		}
		if ( ! IsEmpty((*power_ppit)->expectedMax()) ) {
			myMaxNet(maxNet_v, myNetId);
			myMaxNetId = myMaxNet.finalNetId;
			myExpectedValueFound = false;
			if ( (*power_ppit)->expectedMax() == "open" ) {
				// simVoltage in the following condition is correct. open voltages can have min/max voltages.
				if ( myMaxNetId == UNKNOWN_NET || ! netVoltagePtr_v[myMaxNetId] || netVoltagePtr_v[myMaxNetId]->simVoltage == UNKNOWN_VOLTAGE ) { // open match
					myExpectedValueFound = true;
				}
			// simVoltage in the following condition is correct. open voltages can have min/max voltages.
			} else if ( myMaxNetId != UNKNOWN_NET && netVoltagePtr_v[myMaxNetId] && netVoltagePtr_v[myMaxNetId]->simVoltage != UNKNOWN_VOLTAGE ) {
				if ( String_to_Voltage((*power_ppit)->expectedMax()) == netVoltagePtr_v[myMaxNetId]->maxVoltage ) { // voltage match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedMax() == NetName(myMaxNetId) ) { // name match
					myExpectedValueFound = true;
				} else if ( (*power_ppit)->expectedMax() == string(netVoltagePtr_v[myMaxNetId]->powerAlias()) ) { // alias match
					myExpectedValueFound = true;
				}
			}
			if ( ! myExpectedValueFound ) {
				errorCount[EXPECTED_VOLTAGE]++;
				errorFile << "Expected maximum " << NetName(myNetId) << " = " << (*power_ppit)->expectedMax() << " but found ";
				if ( myMaxNetId != UNKNOWN_NET && netVoltagePtr_v[myMaxNetId] && netVoltagePtr_v[myMaxNetId]->maxVoltage != UNKNOWN_VOLTAGE ) {
					errorFile << NetName(myMaxNetId) << "@" << PrintVoltage(netVoltagePtr_v[myMaxNetId]->maxVoltage) << endl;
				} else {
					errorFile << "unknown" << endl;
				}
			}
		}
		if ( myLastErrorCount != errorCount[EXPECTED_VOLTAGE] ) errorFile << endl;
	}
	errorFile << "! Finished" << endl << endl;
}

void CCvcDb::FindLDDErrors() {
	CFullConnection myConnections;
	for (CModelListMap::iterator keyModelListPair_pit = cvcParameters.cvcModelListMap.begin(); keyModelListPair_pit != cvcParameters.cvcModelListMap.end(); keyModelListPair_pit++) {
		for (CModelList::iterator model_pit = keyModelListPair_pit->second.begin(); model_pit != keyModelListPair_pit->second.end(); model_pit++) {
			if ( model_pit->type == LDDN || model_pit->type == LDDP ) {
				reportFile << "! Checking LDD errors for model: " << model_pit->definition << endl << endl;
				errorFile << "! Checking LDD errors for model: " << model_pit->definition << endl << endl;
				CDevice * myDevice_p = model_pit->firstDevice_p;
				while (myDevice_p) {
					CCircuit * myParent_p = myDevice_p->parent_p;
	//				deviceId_t myLocalDeviceId = myParent_p->localDeviceIdMap[myDevice_p->name];
					for (instanceId_t instance_it = 0; instance_it < myParent_p->instanceId_v.size(); instance_it++) {
						CInstance * myInstance_p = instancePtr_v[myParent_p->instanceId_v[instance_it]];
						MapDeviceNets(myInstance_p, myDevice_p, myConnections);
						if ( model_pit->type == LDDN ) {
							if ( ! myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN, true) ) continue;
							if ( myConnections.maxSourceVoltage <= myConnections.minDrainVoltage ) continue;
							if ( myConnections.minSourceVoltage > myConnections.minDrainVoltage ||
									( myConnections.minSourceVoltage == myConnections.minDrainVoltage &&
										myConnections.masterMinSourceNet.finalResistance > myConnections.masterMinDrainNet.finalResistance ) ||
									myConnections.maxSourceVoltage > myConnections.maxDrainVoltage ||
									( myConnections.maxSourceVoltage == myConnections.maxDrainVoltage &&
										myConnections.masterMaxSourceNet.finalResistance < myConnections.masterMaxDrainNet.finalResistance &&
										myConnections.minSourceVoltage != myConnections.maxDrainVoltage && // if min = max -> no leak regardless of resistance
//										myConnections.masterMaxSourceNet.finalNetId != myConnections.masterMaxDrainNet.finalNetId &&
										myConnections.masterMaxDrainNet.nextNetId != GetEquivalentNet(myConnections.sourceId)) ) { // NMOS max voltage resistance check is intentionally backwards
								if ( !( IsKnownVoltage_(myConnections.simGateVoltage) &&
										myConnections.simGateVoltage <= min(myConnections.minSourceVoltage, myConnections.minDrainVoltage) ) ) {
									errorCount[LDD_SOURCE]++;
									if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
										PrintDeviceWithAllConnections(myParent_p->instanceId_v[instance_it], myConnections, errorFile);
										errorFile << endl;
									}
								}
							}
						} else {
							assert( model_pit->type == LDDP );
							if ( ! myConnections.CheckTerminalMinMaxVoltages(SOURCE | DRAIN, true) ) continue;
							if ( myConnections.minSourceVoltage >= myConnections.maxDrainVoltage ) continue;
							if ( myConnections.maxSourceVoltage < myConnections.maxDrainVoltage ||
									( myConnections.maxSourceVoltage == myConnections.maxDrainVoltage &&
										myConnections.masterMaxSourceNet.finalResistance > myConnections.masterMaxDrainNet.finalResistance ) ||
									myConnections.minSourceVoltage < myConnections.minDrainVoltage ||
									( myConnections.minSourceVoltage == myConnections.minDrainVoltage &&
										myConnections.masterMinSourceNet.finalResistance < myConnections.masterMinDrainNet.finalResistance &&
										myConnections.maxSourceVoltage != myConnections.minDrainVoltage && // if max = min -> no leak regardless of resistance
//										myConnections.masterMinSourceNet.finalNetId != myConnections.masterMinDrainNet.finalNetId &&
										myConnections.masterMinDrainNet.nextNetId != GetEquivalentNet(myConnections.sourceId)) ) { // PMOS max voltage resistance check is intentionally backwards
								if ( !( IsKnownVoltage_(myConnections.simGateVoltage) &&
										myConnections.simGateVoltage >= max(myConnections.maxSourceVoltage, myConnections.maxDrainVoltage) ) ) {
									errorCount[LDD_SOURCE]++;
									if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(myConnections.deviceId) < cvcParameters.cvcCircuitErrorLimit ) {
										PrintDeviceWithAllConnections(myParent_p->instanceId_v[instance_it], myConnections, errorFile);
										errorFile << endl;
									}
								}
							}
						}
					}
					myDevice_p = myDevice_p->nextDevice_p;
				}
			}
		}
	}
	cvcCircuitList.PrintAndResetCircuitErrors(cvcParameters.cvcCircuitErrorLimit, errorFile, "! Checking LDD errors for model: ");
}

/*
	void CCvcDb::ReportBadLddConnection(CEventQueue & theEventQueue, deviceId_t theDeviceId) {
		static CFullConnection myConnections;

		MapDeviceNets(theDeviceId, myConnections);
		errorCount[LDD_SOURCE]++;
		if ( cvcParameters.cvcCircuitErrorLimit == 0 || IncrementDeviceError(theDeviceId) < cvcParameters.cvcCircuitErrorLimit ) {
			errorFile << "! LDD MOS drain->source connection: ";
			if ( theEventQueue.queueType == MIN_QUEUE ) {
				errorFile << PrintVoltage(myConnections.minDrainVoltage) << "->" << PrintVoltage(myConnections.minSourceVoltage) << endl;
				PrintDeviceWithMinConnections(deviceParent_v[theDeviceId], myConnections, errorFile);
			} else if ( theEventQueue.queueType == MAX_QUEUE ) {
				errorFile << PrintVoltage(myConnections.maxDrainVoltage) << "->" << PrintVoltage(myConnections.maxSourceVoltage) << endl;
				PrintDeviceWithMaxConnections(deviceParent_v[theDeviceId], myConnections, errorFile);
			}
			errorFile << endl;
		}
	}
*/


