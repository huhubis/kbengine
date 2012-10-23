/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2012 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "entity_table.hpp"
#include "db_interface.hpp"
#include "entitydef/entitydef.hpp"
#include "entitydef/scriptdef_module.hpp"
#include "thread/threadguard.hpp"

namespace KBEngine { 
KBE_SINGLETON_INIT(EntityTables);

EntityTables g_EntityTables;

//-------------------------------------------------------------------------------------
void EntityTable::addItem(EntityTableItem* pItem)
{
	tableItems_[pItem->utype()].reset(pItem);
}

//-------------------------------------------------------------------------------------
EntityTableItem* EntityTable::findItem(int32/*ENTITY_PROPERTY_UID*/ utype)
{
	TABLEITEM_MAP::iterator iter = tableItems_.find(utype);
	if(iter != tableItems_.end())
	{
		return iter->second.get();
	}

	return NULL;
}

//-------------------------------------------------------------------------------------
bool EntityTable::updateTable(DBID dbid, MemoryStream* s, ScriptDefModule* pModule)
{
	while(s->opsize() > 0)
	{
		ENTITY_PROPERTY_UID pid;
		(*s) >> pid;
		
		EntityTableItem* pTableItem = this->findItem(pid);
		KBE_ASSERT(pTableItem != NULL);

		if(!pTableItem->updateItem(dbid, s, pModule))
			return false;
	};

	return true;
}

//-------------------------------------------------------------------------------------
EntityTables::EntityTables()
{
}

//-------------------------------------------------------------------------------------
EntityTables::~EntityTables()
{
	tables_.clear();
}

//-------------------------------------------------------------------------------------
bool EntityTables::load(DBInterface* dbi)
{
	pdbi_ = dbi;

	EntityDef::SCRIPT_MODULES smodules = EntityDef::getScriptModules();
	EntityDef::SCRIPT_MODULES::const_iterator iter = smodules.begin();
	for(; iter != smodules.end(); iter++)
	{
		ScriptDefModule* pSM = (*iter).get();
		EntityTable* pEtable = dbi->createEntityTable();
		bool ret = pEtable->initialize(pdbi_, pSM, pSM->getName());

		if(!ret)
		{
			delete pEtable;
			return false;
		}

		tables_[pEtable->tableName()].reset(pEtable);
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityTables::syncToDB()
{
	// 开始同步所有表
	EntityTables::TABLES_MAP::iterator iter = tables_.begin();
	for(; iter != tables_.end(); iter++)
	{
		if(!iter->second->syncToDB())
			return false;
	}

	std::vector<std::string> dbTableNames;
	pdbi_->getTableNames(dbTableNames, "");

	// 检查是否有需要删除的表
	std::vector<std::string>::iterator iter0 = dbTableNames.begin();
	for(; iter0 != dbTableNames.end(); iter0++)
	{
		std::string tname = (*iter0);
		if(std::string::npos == tname.find(ENTITY_TABLE_PERFIX"_"))
			continue;

		KBEngine::kbe_replace(tname, ENTITY_TABLE_PERFIX"_", "");
		EntityTables::TABLES_MAP::iterator iter = tables_.find(tname);
		if(iter == tables_.end())
		{
			if(!pdbi_->dropEntityTableFromDB((std::string(ENTITY_TABLE_PERFIX"_") + tname).c_str()))
				return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------------------
void EntityTables::addTable(EntityTable* pTable)
{
	TABLES_MAP::iterator iter = tables_.begin();

	for(; iter != tables_.end(); iter++)
	{
		if(iter->first == pTable->tableName())
		{
			KBE_ASSERT(false && "table exist!\n");
			return;
		}
	}

	tables_[pTable->tableName()].reset(pTable);
}

//-------------------------------------------------------------------------------------
EntityTable* EntityTables::findTable(std::string name)
{
	TABLES_MAP::iterator iter = tables_.find(name);
	if(iter != tables_.end())
	{
		return iter->second.get();
	}

	return NULL;
};

//-------------------------------------------------------------------------------------
bool EntityTables::writeEntity(DBID dbid, MemoryStream* s, ScriptDefModule* pModule)
{
	KBEngine::thread::ThreadGuard tg(&this->logMutex); 

	EntityTable* pTable = this->findTable(pModule->getName());
	KBE_ASSERT(pTable != NULL);

	return pTable->updateTable(dbid, s, pModule);
}

//-------------------------------------------------------------------------------------
}
