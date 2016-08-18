/*
 * CParameterMap.hh
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

#ifndef CPARAMETERMAP_HH_
#define CPARAMETERMAP_HH_

#include "Cvc.hh"

#include "CNormalValue.hh"

class CParameterMap : public map<string, string> {
public:
	void CreateParameterMap(string theParameterString);
	resistance_t CalculateResistance(string theEquation);
	void Print(string theIndentation = "", string theHeading = "");
};

#endif /* CPARAMETERMAP_HH_ */