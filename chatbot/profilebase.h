/*
    This file is part of GGChatBot.

    Copyright (C) 2009  Dariusz Mikulski <dariusz.mikulski@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#ifndef _PROFILEBASE_H
#define	_PROFILEBASE_H

#include "profileptr.h"

class ProfileBase
{
public:
    ProfileBase();
    virtual ~ProfileBase();

    void SetProfile(const ProfilePtr);
    ProfilePtr GetProfile() const;
    
private:
    ProfilePtr m_profile;
};

#endif	/* _PROFILEBASE_H */

