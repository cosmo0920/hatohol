/*
 * Copyright (C) 2013 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ItemTablePtr_h
#define ItemTablePtr_h

#include <list>
#include <map>
#include "UsedCountablePtr.h"
#include "ItemTable.h"

typedef UsedCountablePtr<ItemTable>       VariableItemTablePtr;
typedef UsedCountablePtr<const ItemTable> ItemTablePtr;

typedef std::list<ItemTablePtr>                 ItemTablePtrList;
typedef std::list<ItemTablePtr>::iterator       ItemTablePtrListIterator;
typedef std::list<ItemTablePtr>::const_iterator ItemTablePtrListConstIterator;

template<> VariableItemTablePtr::UsedCountablePtr(void);
template<> ItemTablePtr::UsedCountablePtr(void);

typedef std::map<ItemGroupPtr, VariableItemTablePtr, ItemGroupPtrComparator>
  ItemGroupTableMap;
typedef ItemGroupTableMap::iterator ItemGroupTableMapIterator;

#endif // #define ItemTablePtr_h

