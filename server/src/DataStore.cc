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

#include "DataStore.h"

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
DataStore::DataStore(void)
{
}

void DataStore::setCopyOnDemandEnable(bool enable)
{
}

void DataStore::startOnDemandFetchItem(Closure0 *closure)
{
}

bool DataStore::isFetchItemsSupported(void)
{
	return false;
}

void DataStore::startOnDemandFetchHistory(const ItemInfo &itemInfo,
					  const time_t &beginTime,
					  const time_t &endTime,
					  Closure1<HistoryInfoVect> *closure)
{
	HistoryInfoVect historyInfoVect;
	(*closure)(historyInfoVect);
	delete closure;
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
DataStore::~DataStore()
{
}
