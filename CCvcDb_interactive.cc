/*
 * CCvcDb_interactive.cc
 *
 * Copyright 2014-2106 D. Mitch Bailey  d.mitch.bailey at gmail dot com
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

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <regex>


#include "CCircuit.hh"
#include "CConnection.hh"
#include "CCvcDb.hh"
#include "CCvcExceptions.hh"
#include "CCvcParameters.hh"
#include "CDevice.hh"
#include "CEventQueue.hh"
#include "CInstance.hh"
#include "CModel.hh"
#include "CPower.hh"
#include "Cvc.hh"
#include "CvcTypes.hh"
#include "CVirtualNet.hh"
#include "gzstream.h"

//#include "readline.h"
#include <readline/readline.h>
// extern char * readline(const char * thePrompt);
#include <readline/history.h>
// extern void add_history(char * line_read);

extern int gContinueCount;

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *
rl_gets (string thePrompt)
{
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
    {
      free (line_read);
      line_read = (char *)NULL;
    }

  /* Get a line from the user. */
  line_read = readline (thePrompt.c_str());

  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);

  return (line_read);
}

void CCvcDb::FindInstances(string theSubcircuit, bool thePrintCircuitFlag) {
	size_t myInstanceCount = 0;
	try {
		CCircuit * myCircuit_p = cvcCircuitList.FindCircuit(theSubcircuit);
		for ( auto instance_pit = myCircuit_p->instanceId_v.begin(); instance_pit != myCircuit_p->instanceId_v.end(); instance_pit++ ) {
			reportFile << HierarchyName(*instance_pit, thePrintCircuitFlag) << endl;
			myInstanceCount++;
		}
		reportFile << "found " << myInstanceCount << " instances" << endl;
	}
	catch (const out_of_range& oor_exception) {
		reportFile << "Searching for subcircuits matching " << theSubcircuit << endl;
		size_t myMatchCount = 0;
		vector<string> mySearchList;
		try {
			regex mySearchPattern(FuzzyFilter(theSubcircuit));
			mySearchList.reserve(cvcParameters.cvcSearchLimit);
			for( auto circuit_ppit = cvcCircuitList.begin(); circuit_ppit != cvcCircuitList.end(); circuit_ppit++) {
				if ( regex_match((*circuit_ppit)->name, mySearchPattern) ) {
					if ( myMatchCount++ < cvcParameters.cvcSearchLimit ) {
						mySearchList.push_back(string((*circuit_ppit)->name) + " #instances: " + to_string<uintmax_t>((*circuit_ppit)->instanceCount));
					}
				}
			}
			if ( myMatchCount == 0 ) {
				reportFile << "Could not find any subcircuits matching " << theSubcircuit << endl;
			} else {
				sort(mySearchList.begin(), mySearchList.end());
				for ( size_t myIndex = 0; myIndex < mySearchList.size(); myIndex++ ) {
					reportFile << mySearchList[myIndex] << endl;
	 			}
				reportFile << "Displayed " << mySearchList.size() << "/" << myMatchCount << " matches" << endl;
			}
		}
		catch (const regex_error& myError) {
			reportFile << "regex_error: " << RegexErrorString(myError.code()) << endl;
		}
	}
}

CCircuit * CCvcDb::FindSubcircuit(string theSubcircuit) {
	try {
		return(cvcCircuitList.FindCircuit(theSubcircuit));
	}
	catch (const out_of_range& oor_exception) {
		reportFile << "Could not find subcircuit " << theSubcircuit << endl;
		return(NULL);
	}
}

void CCvcDb::PrintSubcircuitCdl(string theSubcircuit) {

	CCircuit * myCircuit = FindSubcircuit(theSubcircuit);
	if ( myCircuit == NULL ) return;

	string myFileName = theSubcircuit + ".cdl";
	ofstream myCdlFile(myFileName);
	if (myCdlFile.fail()) {
		reportFile << "Could not open " << myFileName << endl;
		return;
	}
	unordered_set<text_t> myPrintedList;
	PrintHierarchicalCdl(myCircuit, myPrintedList, myCdlFile);
	myCdlFile << endl;
	reportFile << "Wrote subcircuit " << theSubcircuit << " to " << theSubcircuit << ".cdl" << endl;
	myCdlFile.close();
}

instanceId_t CCvcDb::FindHierarchy(instanceId_t theCurrentInstanceId, string theHierarchy, bool thePrintUnmatchFlag) {
	// HIERARCHY_DELIMITER is only used to delimit hierarchy
	if ( theHierarchy.substr(0, 1) == HIERARCHY_DELIMITER ) {
		theCurrentInstanceId = 0;
		theHierarchy = theHierarchy.substr(1);
	}
	size_t myStringBegin = 0;
	size_t myStringEnd;
	string myInstanceName;
	string myUnmatchedHierarchy = "";
	instanceId_t myLocalInstanceId;
	while ( myStringBegin <= theHierarchy.length() ) {
		try {
			myStringEnd = theHierarchy.find_first_of(HIERARCHY_DELIMITER, myStringBegin);
			myInstanceName = theHierarchy.substr(myStringBegin, myStringEnd - myStringBegin);
			try {
				myInstanceName = myInstanceName.erase(myInstanceName.find_first_of("("));
			}
			catch (...) {
			}
			if ( ! myUnmatchedHierarchy.empty() ) {
				myInstanceName = myUnmatchedHierarchy + HIERARCHY_DELIMITER + myInstanceName;
			}
//			cerr << "searching for " << myInstanceName << endl;
//			cerr << instancePtr_v[theCurrentInstanceId]->master_p->localDeviceIdMap.size() << " " << cvcCircuitList.cdlText.GetTextAddress(myInstanceName) << endl;
//			for ( auto pair_pit = instancePtr_v[theCurrentInstanceId]->master_p->localDeviceIdMap.begin();
//					pair_pit != instancePtr_v[theCurrentInstanceId]->master_p->localDeviceIdMap.end(); pair_pit++ ) {
//				cerr << pair_pit->first << " " << pair_pit->second << endl;
//			}
			if ( myInstanceName == ".." ) {
				theCurrentInstanceId = instancePtr_v[theCurrentInstanceId]->parentId;
			} else {
				myLocalInstanceId = instancePtr_v[theCurrentInstanceId]->master_p->localDeviceIdMap.at(cvcCircuitList.cdlText.GetTextAddress(myInstanceName));
				theCurrentInstanceId = instancePtr_v[theCurrentInstanceId]->firstSubcircuitId + myLocalInstanceId;
			}
			myUnmatchedHierarchy = "";
		}
		catch (const out_of_range& oor_exception) {
			myUnmatchedHierarchy = myInstanceName;
		}
		myStringBegin = theHierarchy.find_first_not_of(HIERARCHY_DELIMITER, myStringEnd);
	}
	if ( ! myUnmatchedHierarchy.empty() && thePrintUnmatchFlag ) {
		reportFile << "Could not find instance " << myInstanceName << endl;
	}
	return ( theCurrentInstanceId );
}

string CCvcDb::ShortString(netId_t theNetId, bool thePrintSubcircuitNameFlag) {
	string myShortString = "";
	if ( isValidShortData && short_v[theNetId].first != UNKNOWN_NET && short_v[theNetId].first != theNetId ) {
		myShortString += " shorted to " + NetName(short_v[theNetId].first, thePrintSubcircuitNameFlag) + short_v[theNetId].second;
	}
	if ( netVoltagePtr_v[theNetId] ) {
//		myShortString = netVoltagePtr_v[myNetId]->PowerDefinition();
		string myStandardDefinition = netVoltagePtr_v[theNetId]->StandardDefinition();
		if ( IsCalculatedVoltage_(netVoltagePtr_v[theNetId]) ) {
			myShortString += " calculated as";
		} else {
			myShortString += " defined as ";
			if ( netVoltagePtr_v[theNetId]->definition != myStandardDefinition ) {
				myShortString += netVoltagePtr_v[theNetId]->definition + " => ";
			}
		}
		myShortString += netVoltagePtr_v[theNetId]->StandardDefinition();
	}
	return myShortString;
}

void CCvcDb::PrintNets(instanceId_t theCurrentInstanceId, string theFilter, bool thePrintSubcircuitNameFlag, bool theIsValidPowerFlag) {
	CCircuit * myMasterCircuit_p = instancePtr_v[theCurrentInstanceId]->master_p;
	CTextVector mySignal_v;
	stringstream myNetString;
	netId_t myGlobalNetId;
	vector<string> mySearchList;
	size_t myMatchCount = 0;
	try {
		regex mySearchPattern(FuzzyFilter(theFilter));
		mySignal_v.reserve(myMasterCircuit_p->localSignalIdMap.size());
		mySearchList.reserve(mySignal_v.capacity());
		for ( auto pair_pit = myMasterCircuit_p->localSignalIdMap.begin(); pair_pit != myMasterCircuit_p->localSignalIdMap.end(); pair_pit++ ) {
			mySignal_v[pair_pit->second] = pair_pit->first;
		}
		for ( netId_t net_it = 0; net_it < myMasterCircuit_p->localSignalIdMap.size(); net_it++ ) {
			if ( net_it == myMasterCircuit_p->portCount && net_it != 0 ) {
				reportFile << "Ports:" << endl;
				sort(mySearchList.begin(), mySearchList.end());
				for ( size_t myIndex = 0; myIndex < mySearchList.size(); myIndex++ ) {
					reportFile << mySearchList[myIndex] << endl;
				}
				reportFile << "Displayed " << mySearchList.size() << "/" << myMatchCount << " matches" << endl;
				myMatchCount = 0;
				mySearchList.clear();
			}
			myGlobalNetId = instancePtr_v[theCurrentInstanceId]->localToGlobalNetId_v[net_it];
			string	myGlobalNet = "";
			if ( thePrintSubcircuitNameFlag ) {
				myGlobalNet = "(";
				myGlobalNet += NetName(myGlobalNetId, false);
				myGlobalNet += ")";
			}
			myNetString.str("");
			myNetString << mySignal_v[net_it] << myGlobalNet << ((theIsValidPowerFlag) ? ShortString(myGlobalNetId, thePrintSubcircuitNameFlag) : "");
			if ( theFilter.empty() || regex_match(mySignal_v[net_it], mySearchPattern) ) {
				if ( myMatchCount++ < cvcParameters.cvcSearchLimit ) {
					mySearchList.push_back(myNetString.str());
				}
			}
		}
		reportFile << "Internal nets:" << endl;
		sort(mySearchList.begin(), mySearchList.end());
		for ( size_t myIndex = 0; myIndex < mySearchList.size(); myIndex++ ) {
			reportFile << mySearchList[myIndex] << endl;
		}
		reportFile << "Displayed " << mySearchList.size() << "/" << myMatchCount << " matches" << endl;
	}
	catch (const regex_error& myError) {
		reportFile << "regex_error: " << RegexErrorString(myError.code()) << endl;
	}

}

void CCvcDb::PrintDevices(instanceId_t theCurrentInstanceId, string theFilter, bool thePrintSubcircuitNameFlag, bool theIsValidModelFlag) {
	string	myParameters = "";
	stringstream myDeviceString;
	vector<string> mySearchList;
	size_t myMatchCount = 0;
	try {
		regex mySearchPattern(FuzzyFilter(theFilter));
		CCircuit * myMasterCircuit_p = instancePtr_v[theCurrentInstanceId]->master_p;
		mySearchList.reserve(myMasterCircuit_p->devicePtr_v.size());
		for ( auto device_ppit = myMasterCircuit_p->devicePtr_v.begin();
				device_ppit != myMasterCircuit_p->devicePtr_v.end(); device_ppit++ ) {
			if ( thePrintSubcircuitNameFlag ) {
				myParameters = "(";
				myParameters += (*device_ppit)->parameters;
				myParameters += ")";
			}
			myDeviceString.str("");
			myDeviceString << (*device_ppit)->name << myParameters << " " << ((theIsValidModelFlag) ? (*device_ppit)->model_p->definition : "" );
			if ( theFilter.empty() || regex_match((*device_ppit)->name, mySearchPattern) ) {
				if ( myMatchCount++ < cvcParameters.cvcSearchLimit ) {
					mySearchList.push_back(myDeviceString.str());
				}
			}
		}
		sort(mySearchList.begin(), mySearchList.end());
		for ( size_t myIndex = 0; myIndex < mySearchList.size(); myIndex++ ) {
			reportFile << mySearchList[myIndex] << endl;
		}
		reportFile << "Displayed " << mySearchList.size() << "/" << myMatchCount << " matches" << endl;
	}
	catch (const regex_error& myError) {
		reportFile << "regex_error: " << RegexErrorString(myError.code()) << endl;
	}
}

void CCvcDb::PrintInstances(instanceId_t theCurrentInstanceId, string theFilter, bool thePrintSubcircuitNameFlag) {
	string	myMasterName = "";
	stringstream myInstanceString;
	vector<string> mySearchList;
	size_t myMatchCount = 0;
	try {
		regex mySearchPattern(FuzzyFilter(theFilter));
		CCircuit * myMasterCircuit_p = instancePtr_v[theCurrentInstanceId]->master_p;
		mySearchList.reserve(myMasterCircuit_p->subcircuitPtr_v.size());
		for ( auto subcircuit_ppit = myMasterCircuit_p->subcircuitPtr_v.begin();
				subcircuit_ppit != myMasterCircuit_p->subcircuitPtr_v.end(); subcircuit_ppit++ ) {
			if ( thePrintSubcircuitNameFlag ) {
				myMasterName = "(";
				myMasterName += (*subcircuit_ppit)->masterName;
				myMasterName += ")";
			}
			myInstanceString.str("");
			myInstanceString << (*subcircuit_ppit)->name << myMasterName;
			if ( theFilter.empty() || regex_match((*subcircuit_ppit)->name, mySearchPattern) ) {
				if ( myMatchCount++ < cvcParameters.cvcSearchLimit ) {
					mySearchList.push_back(myInstanceString.str());
				}
			}
		}
		sort(mySearchList.begin(), mySearchList.end());
		for ( size_t myIndex = 0; myIndex < mySearchList.size(); myIndex++ ) {
			reportFile << mySearchList[myIndex] << endl;
		}
		reportFile << "Displayed " << mySearchList.size() << "/" << myMatchCount << " matches" << endl;
	}
	catch (const regex_error& myError) {
		reportFile << "regex_error: " << RegexErrorString(myError.code()) << endl;
	}
}

void CCvcDb::ReadShorts(string theShortFileName) {
	// TODO: make sure short output applies to current data base
	igzstream myShortFile(theShortFileName);
	if ( myShortFile.fail() ) {
		reportFile << "ERROR: Could not open " << theShortFileName << endl;
		return;
	}
	string	myInputLine;
	char	myValue[256];
	netId_t myNetId, myShortId;
//	voltage_t myVoltage;
//	resistance_t myResistance;
	int myInputCount;
	try {
		while ( getline(myShortFile, myInputLine) ) {
			myValue[0] = '\0';
			myInputCount = sscanf(myInputLine.c_str(), "%u->%u%s", &myNetId, &myShortId, myValue);
			if ( myInputCount == 1 ) {
				myInputCount = sscanf(myInputLine.c_str(), "%u%s", &myNetId, myValue);
				myShortId = myNetId;
			}
			if ( myNetId >= netCount ) throw EShortFileError();
			short_v[myNetId].first = myShortId;
			short_v[myNetId].second = myValue;
		}
		isValidShortData = true;
	}
	catch (exception& e) {
		reportFile << "Error reading short file" << endl;
		isValidShortData = false;
		ResetVector<CShortVector>(short_v, netCount);
	}
}

netId_t CCvcDb::FindNet(instanceId_t theCurrentInstanceId, string theNetName, bool theDisplayErrorFlag) {
	instanceId_t myCurrentInstanceId;
	string myInitialHierarchy;
	string mySearchHierarchy;
	if (theNetName.length() > theNetName.find_last_of(HIERARCHY_DELIMITER) ) {
		mySearchHierarchy = theNetName.substr(0, theNetName.find_last_of(HIERARCHY_DELIMITER));
	} else {
		mySearchHierarchy = "";
	}
	string myNetName;
	try {
		if ( theNetName.substr(0, 1) == HIERARCHY_DELIMITER ) {
			myInitialHierarchy = "";
			myCurrentInstanceId = FindHierarchy(0, mySearchHierarchy, false);
		} else {
			myInitialHierarchy = HierarchyName(theCurrentInstanceId, false) + HIERARCHY_DELIMITER;
			myCurrentInstanceId = FindHierarchy(theCurrentInstanceId, mySearchHierarchy, false);
		}
		CCircuit * myCircuit_p = instancePtr_v[myCurrentInstanceId]->master_p;
		string myParentName = HierarchyName(myCurrentInstanceId, false) + HIERARCHY_DELIMITER;
		myNetName = theNetName.substr(myParentName.length() - myInitialHierarchy.length());
		return instancePtr_v[myCurrentInstanceId]->localToGlobalNetId_v[myCircuit_p->localSignalIdMap.at(cvcCircuitList.cdlText.GetTextAddress(myNetName))];
	}
	catch (const out_of_range& oor_exception) {
		if ( theDisplayErrorFlag ) {
			reportFile << "Could not find net " << HierarchyName(myCurrentInstanceId) << HIERARCHY_DELIMITER << myNetName << endl;
		}
	}
	return ( UNKNOWN_NET );
}

deviceId_t CCvcDb::FindDevice(instanceId_t theCurrentInstanceId, string theDeviceName) {
	instanceId_t myCurrentInstanceId;
	string myInitialHierarchy;
	string mySearchHierarchy;
	if (theDeviceName.length() > theDeviceName.find_last_of(HIERARCHY_DELIMITER) ) {
		mySearchHierarchy = theDeviceName.substr(0, theDeviceName.find_last_of(HIERARCHY_DELIMITER));
	} else {
		mySearchHierarchy = "";
	}
	string myDeviceName;
	try {
		if ( theDeviceName.substr(0, 1) == HIERARCHY_DELIMITER ) {
			myInitialHierarchy = "";
			myCurrentInstanceId = FindHierarchy(0, mySearchHierarchy, false);
		} else {
			myInitialHierarchy = HierarchyName(theCurrentInstanceId, false) + HIERARCHY_DELIMITER;
			myCurrentInstanceId = FindHierarchy(theCurrentInstanceId, mySearchHierarchy, false);
		}
		CCircuit * myCircuit_p = instancePtr_v[myCurrentInstanceId]->master_p;
		string myParentName = HierarchyName(myCurrentInstanceId, false) + HIERARCHY_DELIMITER;
		myDeviceName = theDeviceName.substr(myParentName.length() - myInitialHierarchy.length());
		return instancePtr_v[myCurrentInstanceId]->firstDeviceId + myCircuit_p->localDeviceIdMap.at(cvcCircuitList.cdlText.GetTextAddress(myDeviceName));
	}
	catch (const out_of_range& oor_exception) {
		reportFile << "Could not find device " << HierarchyName(myCurrentInstanceId) << HIERARCHY_DELIMITER << myDeviceName << endl;
	}
	return ( UNKNOWN_DEVICE );
}

returnCode_t CCvcDb::InteractiveCvc(int theCurrentStage) {
	string	myInputLine;
	char *	myInput;
	char myInputBuffer[1024];
	istringstream myInputStream;
	string	myCommand;
	string 	myOption;
	string	myHierarchy;
	string	myFilter;
	string	mySubcircuit;
	string	myFileName;
	string	myName;
	string	myNumberString;
	string	myCommandMode = "";
	list<streambuf *> mySavedBufferStack;
	ifstream *myBatchFile;
	bool myIsBatchInput = false;
	int mySearchLimit;
	static instanceId_t myCurrentInstanceId = 0;
	bool	myPrintSubcircuitNameFlag = false;
	size_t	myNumber;
	returnCode_t		myReturnCode = UNKNOWN_RETURN_CODE;
	stringstream	myPrompt;

	while ( myReturnCode == UNKNOWN_RETURN_CODE ) {
//		cout << "** Stage " << theCurrentStage << "/" << STAGE_COMPLETE << ": Enter command ?> ";
		reportFile.flush();
		myPrompt.clear();
		myPrompt.str("");
		if ( myCommandMode == "fs" ) {
			myPrompt << "--> find subcircuit (^D to exit) ?> ";
		} else if ( myCommandMode == "pd" ) {
			myPrompt << "--> print device (^D to exit) ?> ";
		} else if ( myCommandMode == "ph" ) {
			myPrompt << "--> print hierarchy (^D to exit) ?> ";
		} else if ( myCommandMode == "pi" ) {
			myPrompt << "--> print instance (^D to exit) ?> ";
		} else if ( myCommandMode == "pn" ) {
			myPrompt << "--> print net (^D to exit) ?> ";
		} else if ( myCommandMode == "gd" ) {
			myPrompt << "--> get device (^D to exit) ?> ";
		} else if ( myCommandMode == "gh" ) {
			myPrompt << "--> get hierarchy (^D to exit) ?> ";
		} else if ( myCommandMode == "gi" ) {
			myPrompt << "--> get instance (^D to exit) ?> ";
		} else if ( myCommandMode == "gn" ) {
			myPrompt << "--> get net (^D to exit) ?> ";
		} else {
			myPrompt << "** Stage " << theCurrentStage << "/" << STAGE_COMPLETE << ": Enter command ?> ";
		}
//		cin >> myCommand;
		if ( myIsBatchInput ) {
			cin.getline(myInputBuffer, 1024);
			if ( cin.eof() ) {
				myInput = NULL;
			} else {
				myInput = myInputBuffer;
			}
		} else {
			myInput = rl_gets(myPrompt.str());
		}
//		cout << myInput << endl;
		if ( myInput == NULL ) { // eof
//				myCurrentInstanceId = 0; // reset saved hierarchy
			if ( myIsBatchInput ) {
				reportFile << "finished source. Depth " << mySavedBufferStack.size() << endl;
				cin.rdbuf(mySavedBufferStack.front());
				mySavedBufferStack.pop_front();
				myCommandMode = "";
				if ( mySavedBufferStack.empty() ) {
					myIsBatchInput = false;
				}
			} else if ( myCommandMode.empty() ){
				gInteractive_cvc = false;
				cin.clear();
				myReturnCode = OK;
			} else {
				myCommandMode = "";
			}
			reportFile << endl;
			continue;
		}
		reportFile << endl << "> " << myInput << endl;
		try {
//			if ( cin.eof() ) {
			myInputLine = myInput;
			myInputStream.str(myInputLine);
			myInputStream.clear();
//			cout << "stream = " << myInputStream.str() << endl;
			if ( myCommandMode.empty() ) {
				if ( ! (myInputStream >> myCommand) ) continue;
			} else {
				myCommand = myCommandMode;
			}
//			cout << "command = " << myCommand << endl;
			if ( myCommand == "findsubcircuit" || myCommand == "fs"  ) {
				if ( myInputStream >> mySubcircuit ) {
					FindInstances(mySubcircuit, myPrintSubcircuitNameFlag);
				} else {
					myCommandMode = "fs";
				}
			} else if ( myCommand == "goto" || myCommand == "g" || myCommand == "cd" ) {
				myHierarchy = "/"; // default returns to top
				myInputStream >> myHierarchy;
				myCurrentInstanceId = FindHierarchy(myCurrentInstanceId, RemoveCellNames(myHierarchy));
//				myCommand = "pwd"; // after goto, print hierarchy
//				continue;
			} else if ( myCommand == "help" || myCommand == "h" ) {
				cout << "Available commands are:" << endl;
				cout << "<ctrl-d> switch to automatic (i.e. end interactive)" << endl;
				cout << "searchlimit<sl> [limit]: set search limit" << endl;
				cout << "goto<g|cd> <hierarchy>: goto hierarchy" << endl;
				cout << "currenthierarchy<ch|pwd>: print current hierarchy" << endl;
				cout << "printhierarchy<ph> hierarchynumber: print hierarchy name" << endl;
				cout << "printdevice<pd> devicenumber: print device name" << endl;
				cout << "printnet<pn> netnumber: print net name" << endl;
				cout << "listnet|listdevice|listinstance<ln|ld|li> filter: list net|device|instances in current subcircuit filtered by filter" << endl;
				cout << "getnet|getdevice|getinstance<gn|gd|gi> name: get net|device|instance number for name" << endl;
				cout << "dumpfuse<df> filname: dump fuse to filename" << endl;
				cout << "findsubcircuit<fs> subcircuit: list all instances of subcircuit" << endl;
				cout << "printcdl<pc> subcircuit: print subcircuit as subcircuit.cdl" << endl;
				cout << "printenvironment<pe>: print simulation environment" << endl;
				cout << "togglename<n>: toggle subcircuit names" << endl;
//				cout << "shortfile<s> file: use file as shorts" << endl;
				cout << "setpower<sp> file: use file as power" << endl;
				cout << "setmodel<sm> file: use file as model" << endl;
				cout << "setfuse<sf> file: use file as fuse overrides" << endl;
				cout << "printpower<pp>: print power settings" << endl;
				cout << "printmodel<pm>: print model statistics" << endl;
				cout << "source file: read commands from file" << endl;
				cout << "skip: skip this cvcrc and use next one" << endl;
				cout << "rerun: rerun this cvcrc" << endl;
				cout << "continue<c>: continue" << endl;
				cout << "help<h>: help" << endl;
				cout << "quit<q>: quit" << endl;
			} else if ( myCommand == "source" ) {
				myFileName = "";
				myInputStream >> myFileName;
				myBatchFile = new ifstream(myFileName);
				if ( myBatchFile && myBatchFile->good() ) {
					myIsBatchInput = true;
					mySavedBufferStack.push_front(cin.rdbuf());
					cout << "sourcing from " << myFileName << ". Depth " << mySavedBufferStack.size() << endl;
					cin.rdbuf(myBatchFile->rdbuf());
				} else {
					reportFile << "Could not open " << myFileName << endl;
				}
			} else if ( myCommand == "setmodel" || myCommand == "sm" ) {
				if ( theCurrentStage != 1 ) {
					reportFile << "ERROR: Can only change model file at stage 1." << endl;
					continue;
				}
				myFileName = "";
				myInputStream >> myFileName;
				cvcParameters.cvcModelFilename = myFileName;
				if ( ( modelFileStatus = cvcParameters.LoadModels() ) == OK ) {
					modelFileStatus = SetDeviceModels();
				}
				if ( modelFileStatus == OK ) {
					fuseFileStatus = CheckFuses();
				} else if ( ! cvcParameters.cvcFuseFilename.empty() ){
					reportFile << "WARNING: fuse file not checked due to invalid model file" << endl;
					fuseFileStatus = SKIP;
				}
			} else if ( myCommand == "setpower" || myCommand == "sp" ) {
				if ( theCurrentStage != 1 ) {
					reportFile << "ERROR: Can only change power file at stage 1." << endl;
					continue;
				}
				myFileName = "";
				myInputStream >> myFileName;
				cvcParameters.cvcPowerFilename = myFileName;
				if ( ( powerFileStatus = cvcParameters.LoadPower() ) == OK ) {
					powerFileStatus = SetModePower();
				}
			} else if ( myCommand == "setfuse" || myCommand == "sf" ) {
				if ( theCurrentStage != 1 ) {
					reportFile << "ERROR: Can only change fuse file at stage 1." << endl;
					continue;
				}
				myFileName = "";
				myInputStream >> myFileName;
				cvcParameters.cvcFuseFilename = myFileName;
				if ( modelFileStatus == OK ) {
					fuseFileStatus = CheckFuses();
				} else if ( ! cvcParameters.cvcFuseFilename.empty() ){
					reportFile << "WARNING: fuse file not checked due to invalid model file" << endl;
					fuseFileStatus = SKIP;
				}
			} else if ( myCommand == "searchlimit" || myCommand == "sl" ) {
				if ( myInputStream >> mySearchLimit ) {
					cvcParameters.cvcSearchLimit = mySearchLimit;
					reportFile << "Search limit set to: " << cvcParameters.cvcSearchLimit << endl;
				} else {
					reportFile << "Current search limit: " << cvcParameters.cvcSearchLimit << endl;
				}
			} else if ( myCommand == "printmodel" || myCommand == "pm" ) {
				cvcParameters.cvcModelListMap.Print(reportFile);
			} else if ( myCommand == "printpower" || myCommand == "pp" ) {
				PrintPowerList(reportFile);
			} else if ( myCommand == "printcdl" || myCommand == "pc" ) {
				mySubcircuit = "";
				myInputStream >> mySubcircuit;
				PrintSubcircuitCdl(mySubcircuit);
			} else if ( myCommand == "printenvironment" || myCommand == "pe" ) {
				cvcParameters.PrintEnvironment(reportFile);
			} else if ( myCommand == "listnet" || myCommand == "ln" ) {
				if ( myInputStream >> myFilter ) {
					PrintNets(myCurrentInstanceId, myFilter, myPrintSubcircuitNameFlag, powerFileStatus == OK);
				} else {
					PrintNets(myCurrentInstanceId, "", myPrintSubcircuitNameFlag, powerFileStatus == OK);
				}
			} else if ( myCommand == "listdevice" || myCommand == "ld" ) {
				if ( myInputStream >> myFilter ) {
					PrintDevices(myCurrentInstanceId, myFilter, myPrintSubcircuitNameFlag, modelFileStatus == OK);
				} else {
					PrintDevices(myCurrentInstanceId, "", myPrintSubcircuitNameFlag, modelFileStatus == OK);
				}
			} else if ( myCommand == "listinstance" || myCommand == "li" ) {
				if ( myInputStream >> myFilter ) {
					PrintInstances(myCurrentInstanceId, myFilter, myPrintSubcircuitNameFlag);
				} else {
					PrintInstances(myCurrentInstanceId, "", myPrintSubcircuitNameFlag);
				}
	//			myInputStream >> myOption, myFilter;
			} else if ( myCommand == "dumpfuse" || myCommand == "df" ) {
				if ( myInputStream >> myFileName ) {
					if ( modelFileStatus == OK ) {
						DumpFuses(myFileName);
					} else {
						reportFile << "ERROR: Cannot dump fuses because model file is invalid" << endl;
					}
				} else {
					reportFile << "ERROR: no fuse file name" << endl;
				}
			} else if ( myCommand == "togglename" || myCommand == "n" ) {
				myPrintSubcircuitNameFlag = ! myPrintSubcircuitNameFlag;
				reportFile << "Printing subcircuit name option is now " << ((myPrintSubcircuitNameFlag) ? "on" : "off") << endl;
			} else if ( myCommand == "currenthierarchy" || myCommand == "ch" || myCommand == "pwd" ) {
				reportFile << "Current hierarchy(" << myCurrentInstanceId << "): " << HierarchyName(myCurrentInstanceId, myPrintSubcircuitNameFlag) << endl;
			} else if ( myCommand == "printhierarchy" || myCommand == "ph" ) {
				myNumberString = "";
				if ( myInputStream >> myNumberString ) {
					myNumber = from_string<size_t>(myNumberString);
					reportFile << "Current hierarchy(" << myNumber << "): " << HierarchyName(myNumber, myPrintSubcircuitNameFlag) << endl;
				} else {
					myCommandMode = "ph";
				}
			} else if ( myCommand == "printdevice" || myCommand == "pd" ) {
				myNumberString = "";
				if ( myInputStream >> myNumberString ) {
					myNumber = from_string<size_t>(myNumberString);
					reportFile << "Device " << myNumber << ": " << DeviceName(myNumber, myPrintSubcircuitNameFlag) << endl;
				} else {
					myCommandMode = "pd";
				}
			} else if ( myCommand == "printnet" || myCommand == "pn" ) {
				myNumberString = "";
				if ( myInputStream >> myNumberString ) {
					myNumber = from_string<size_t>(myNumberString);
					reportFile << "Net " << myNumber << ": " << NetName(myNumber, myPrintSubcircuitNameFlag) << endl;
				} else {
					myCommandMode = "pn";
				}
			} else if ( myCommand == "getinstance" || myCommand == "gi" ) {
			} else if ( myCommand == "getdevice" || myCommand == "gd" ) {
				myName = "";
				if ( myInputStream >> myName ) {
					netId_t myDeviceId = FindDevice(myCurrentInstanceId, RemoveCellNames(myName));
					if ( myDeviceId != UNKNOWN_DEVICE ) {
						CInstance * myParent_p = instancePtr_v[deviceParent_v[myDeviceId]];
						CDevice * myDevice_p = myParent_p->master_p->devicePtr_v[myDeviceId - myParent_p->firstDeviceId];
						reportFile << "Device " << DeviceName(myDeviceId, myPrintSubcircuitNameFlag) << ": " << myDeviceId;
						reportFile << " " << myDevice_p->parameters << " R=" << parameterResistanceMap[myDevice_p->parameters] << endl;
						reportFile << "  Model " << myDevice_p->model_p->definition << endl;
					}
				} else {
					myCommandMode = "gd";
				}
			} else if ( myCommand == "getnet" || myCommand == "gn" ) {
				myName = "";
				if ( myInputStream >> myName ) {
					netId_t myNetId = FindNet(myCurrentInstanceId, RemoveCellNames(myName));
					if ( myNetId != UNKNOWN_NET ) {
						reportFile << "Net " << NetName(myNetId, myPrintSubcircuitNameFlag) << ": " << myNetId << endl;
						netId_t myEquivalentNetId = (isFixedEquivalentNet) ? GetEquivalentNet(myNetId) : myNetId;
						string mySimpleName = NetName(myEquivalentNetId, PRINT_CIRCUIT_OFF);
						if ( powerFileStatus == OK && netVoltagePtr_v[myEquivalentNetId] ) {
							if ( netVoltagePtr_v[myEquivalentNetId]->powerSignal != mySimpleName ) {
								reportFile << " base definition " << netVoltagePtr_v[myEquivalentNetId]->powerSignal;
							}
							reportFile << ShortString(myEquivalentNetId, myPrintSubcircuitNameFlag) << endl;
						}
						reportFile << endl;
						if ( theCurrentStage >= STAGE_COMPLETE ) {
							if ( minLeakNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CVirtualLeakNetVector>(minLeakNet_v, myNetId, "Initial min path", reportFile);
							if ( maxLeakNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CVirtualLeakNetVector>(maxLeakNet_v, myNetId, "Initial max path", reportFile);
						}
						if ( theCurrentStage >= STAGE_SECOND_SIM ) {
							if ( initialSimNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CBaseVirtualNetVector>(initialSimNet_v, myNetId, "Initial sim path", reportFile);
	/* 3pass
							if ( logicMinNet_v[myNetId].nextNetId != myNetId ) 	PrintVirtualNet(logicMinNet_v, myNetId, "Logic min path", cout);
							if ( logicMaxNet_v[myNetId].nextNetId != myNetId ) 	PrintVirtualNet(logicMaxNet_v, myNetId, "Logic max path", cout);
							if ( logicSimNet_v[myNetId].nextNetId != myNetId ) 	PrintVirtualNet(logicSimNet_v, myNetId, "Logic sim path", cout);
	*/
						}
						if ( theCurrentStage >= STAGE_RESISTANCE ) {

							if ( minNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CVirtualNetVector>(minNet_v, myNetId, "Min path", reportFile);
							if ( maxNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CVirtualNetVector>(maxNet_v, myNetId, "Max path", reportFile);
							if ( simNet_v[myEquivalentNetId].nextNetId != myNetId ) 	PrintVirtualNet<CVirtualNetVector>(simNet_v, myNetId, "Sim path", reportFile);
						}
					}
				} else {
					myCommandMode = "gn";
				}
			} else if ( myCommand == "skip" ) {
				myCurrentInstanceId = 0; // reset saved hierarchy
				myReturnCode = SKIP;
			} else if ( myCommand == "rerun" ) {
				myCurrentInstanceId = 0; // reset saved hierarchy
				cvcArgIndex--;
				logFile.close();
				errorFile.close();
				myReturnCode = SKIP;
//			} else if ( myCommand == "shortfile" || myCommand == "s" ) {
//				myFileName = "";
//				myInputStream >> myFileName;
//				ReadShorts(myFileName);
			} else if ( myCommand == "c" || myCommand == "continue" ) {
				if ( theCurrentStage == STAGE_COMPLETE && cvcArgIndex + 1 >= cvcArgCount ) { // attempt to continue past last cvcrc
					reportFile << "This is the last cvcrc file. Use 'rerun' or 'quit'." << endl;
				} else { // valid continue
					myReturnCode = OK;
					if ( myInputStream >> gContinueCount ) {
						;
					} else {
						gContinueCount = 1;
					}
					cout << "continuing for " << gContinueCount << " step(s)" << endl;
				}
			} else if ( myCommand == "q" || myCommand == "quit" ) {
				RemoveLock();
				exit(0);
			} else {
				reportFile << "Unrecognized command '" << myCommand << "'" << endl;
			}
		}
		catch (invalid_argument& e) {
			reportFile << "invalid argument" << endl;
		}
	}
	return(myReturnCode);
}

void CCvcDb::DumpFuses(string theFileName) {
	ofstream myDumpFile(theFileName);
	if ( myDumpFile.fail() ) {
		reportFile << "ERROR: Could not open " << theFileName << endl;
		return;
	}
	reportFile << "Dumping fuses to " << theFileName << " ... "; cout.flush();
	size_t myFuseCount = 0;
	for (CModelListMap::iterator keyModelListPair_pit = cvcParameters.cvcModelListMap.begin(); keyModelListPair_pit != cvcParameters.cvcModelListMap.end(); keyModelListPair_pit++) {
		for (CModelList::iterator model_pit = keyModelListPair_pit->second.begin(); model_pit != keyModelListPair_pit->second.end(); model_pit++) {
			if ( model_pit->type == FUSE_ON || model_pit->type == FUSE_OFF ) {
				for (CDevice * device_pit = model_pit->firstDevice_p; device_pit != NULL; device_pit = device_pit->nextDevice_p) {
					CCircuit * myParent_p = device_pit->parent_p;
					for (instanceId_t instance_it = 0; instance_it < myParent_p->instanceId_v.size(); instance_it++) {
						myDumpFile << DeviceName(instancePtr_v[myParent_p->instanceId_v[instance_it]]->firstDeviceId + myParent_p->localDeviceIdMap[device_pit->name]) << " " << gModelTypeMap[model_pit->type] << endl;
						myFuseCount++;
					}
				}
			}
		}
	}
	reportFile << "total fuses written: " << myFuseCount << endl;
	myDumpFile.close();
}

returnCode_t CCvcDb::CheckFuses() {
	if ( cvcParameters.cvcFuseFilename.empty() ) return (OK);
	ifstream myFuseFile(cvcParameters.cvcFuseFilename);
	if ( myFuseFile.fail() ) {
		reportFile << "ERROR: Could not open fuse file: " << cvcParameters.cvcFuseFilename << endl;
		return(FAIL);
	}
	string myFuseName, myFuseStatus;
	bool myFuseError = false;
	deviceId_t myDeviceId;
	string myInputLine;
	while ( getline(myFuseFile, myInputLine) ) {
		if ( myInputLine.length() == 0 ) continue;
		if ( myInputLine.substr(0, 1) == "#" ) continue;
		myInputLine = trim_(myInputLine);
		stringstream myFuseStringStream(myInputLine);
		myFuseStringStream >> myFuseName >> myFuseStatus;
		if ( myFuseStringStream.fail() ) {
			reportFile << "ERROR: invalid format '" << myInputLine << "' expected fuse_name fuse_on/fuse_off" << endl;
			myFuseError = true;
			continue;
		}
		myDeviceId = FindDevice(0, myFuseName);
		if ( myFuseStatus != "fuse_on" && myFuseStatus != "fuse_off" ) {
			reportFile << "ERROR: unknown fuse setting " << myFuseStatus << endl;
			myFuseError = true;
		}
		if ( myDeviceId == UNKNOWN_DEVICE ) {
			reportFile << "ERROR: unknown device " << myFuseName << endl;
			myFuseError = true;
		} else {
			CInstance * myInstance_p = instancePtr_v[deviceParent_v[myDeviceId]];
			CCircuit * myCircuit_p = myInstance_p->master_p;
			CModel * myModel_p = myCircuit_p->devicePtr_v[myDeviceId - myInstance_p->firstDeviceId]->model_p;
			if ( myModel_p->type != FUSE_ON && myModel_p->type != FUSE_OFF ) {
				reportFile << "ERROR: not a fuse " << myFuseName << endl;
				myFuseError = true;
			}
		}
	}
	myFuseFile.close();
	return ((myFuseError) ? FAIL : OK);
}
