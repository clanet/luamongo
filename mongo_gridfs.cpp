#include <iostream>
#include <mongo/client/dbclient.h>
#include <mongo/client/gridfs.h>
#include "utils.h"
#include "common.h"

// FIXME: GridFS pointer is keep in GridFile objects, review Lua binding in
// order to avoid unexpected garbage collection of GridFS pointer

using namespace mongo;

extern void lua_to_bson(lua_State *L, int stackpos, BSONObj &obj);
extern void bson_to_lua(lua_State *L, const BSONObj &obj);
extern int gridfile_create(lua_State *L, GridFile gf);
extern DBClientBase* userdata_to_dbclient(lua_State *L, int stackpos);

namespace {
    inline GridFS* userdata_to_gridfs(lua_State* L, int index) {
        void *ud = 0;

        ud = luaL_checkudata(L, index, LUAMONGO_GRIDFS);
        GridFS *gridfs = *((GridFS **)ud);

        return gridfs;
    }
} // anonymous namespace

/*
 * gridfs, err = mongo.GridFS.New(connection, dbname[, prefix])
 */
static int gridfs_new(lua_State *L) {
    int n = lua_gettop(L);
    int resultcount = 1;

    try {
        DBClientBase *connection = userdata_to_dbclient(L, 1);
        const char *dbname = lua_tostring(L, 2);

        GridFS **gridfs = (GridFS **)lua_newuserdata(L, sizeof(GridFS *));

        if (n >= 3) {
            const char *prefix = luaL_checkstring(L, 3);

            *gridfs = new GridFS(*connection, dbname, prefix);
        } else {
            *gridfs = new GridFS(*connection, dbname);
        }

        luaL_getmetatable(L, LUAMONGO_GRIDFS);
        lua_setmetatable(L, -2);
    } catch (std::exception &e) {
        lua_pushnil(L);
        lua_pushfstring(L, LUAMONGO_ERR_GRIDFS_FAILED, e.what());
        resultcount = 2;
    }

    return resultcount;
}



/*
 * gridfile, err = gridfs:find_file(filename)
 */
static int gridfs_find_file(lua_State *L) {
    GridFS *gridfs = userdata_to_gridfs(L, 1);
    int resultcount = 1;

    if (!lua_isnoneornil(L, 2)) {
        try {
            int type = lua_type(L, 2);
            if (type == LUA_TTABLE) {
                BSONObj obj;
                lua_to_bson(L, 2, obj);
                GridFile gridfile = gridfs->findFile(obj);
                resultcount = gridfile_create(L, gridfile);
            } else {
                GridFile gridfile = gridfs->findFile(luaL_checkstring(L, 2));
                resultcount = gridfile_create(L, gridfile);
            }

        } catch (std::exception &e) {
            lua_pushboolean(L, 0);
            lua_pushfstring(L, LUAMONGO_ERR_CALLING, LUAMONGO_GRIDFS, "find_file", e.what());
            resultcount = 2;
        }
    }

    return resultcount;

}

/*
 * cursor,err = gridfs:list([lua_table or json_str])
 */
static int gridfs_list(lua_State *L) {
    GridFS *gridfs = userdata_to_gridfs(L, 1);

    BSONObj query;
    int type = lua_type(L, 2);
    if (type == LUA_TSTRING) {
        const char *jsonstr = luaL_checkstring(L, 2);
        query = fromjson(jsonstr);
    } else if (type == LUA_TTABLE) {
        lua_to_bson(L, 2, query);
    }
    std::auto_ptr<DBClientCursor> autocursor = gridfs->list(query);

    if (!autocursor.get()){
        lua_pushnil(L);
        lua_pushstring(L, LUAMONGO_ERR_CONNECTION_LOST);
        return 2;
    }

    DBClientCursor **cursor = (DBClientCursor **)lua_newuserdata(L, sizeof(DBClientCursor *));
    *cursor = autocursor.get();
    autocursor.release();

    luaL_getmetatable(L, LUAMONGO_CURSOR);
    lua_setmetatable(L, -2);

    return 1;
}

/*
 * ok, err = gridfs:remove_file(filename)
 */
static int gridfs_remove_file(lua_State *L) {
    int resultcount = 1;

    GridFS *gridfs = userdata_to_gridfs(L, 1);

    const char *filename = luaL_checkstring(L, 2);

    try {
        gridfs->removeFile(filename);
        lua_pushboolean(L, 1);
    } catch (std::exception &e) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, LUAMONGO_ERR_CALLING, LUAMONGO_GRIDFS, "remove_file", e.what());
        resultcount = 2;
    }

    return resultcount;
}

/*
 * bson, err = gridfs:store_file(filename[, remote_file[, content_type]])
 */
static int gridfs_store_file(lua_State *L) {
    int resultcount = 1;

    GridFS *gridfs = userdata_to_gridfs(L, 1);

    const char *filename = luaL_checkstring(L, 2);
    const char *remote = luaL_optstring(L, 3, "");
    const char *content_type = luaL_optstring(L, 4, "");

    try {
        BSONObj res = gridfs->storeFile(filename, remote, content_type);
        bson_to_lua(L, res);
    } catch (std::exception &e) {
        lua_pushnil(L);
        lua_pushfstring(L, LUAMONGO_ERR_CALLING, LUAMONGO_GRIDFS, "store_file", e.what());
        resultcount = 2;
    }

    return resultcount;
}


/*
 * bson, err = gridfs:store_data(data[, remote_file], content_type]])
 * puts the file represented by data into the db
 */
static int gridfs_store_data(lua_State *L) {
    int resultcount = 1;

    GridFS *gridfs = userdata_to_gridfs(L, 1);

    size_t length = 0;
    const char *data = luaL_checklstring(L, 2, &length);
    const char *remote = luaL_optstring(L, 3, "");
    const char *content_type = luaL_optstring(L, 4, "");

    try {
        BSONObj res = gridfs->storeFile(data, length, remote, content_type);
        bson_to_lua(L, res);
    } catch (std::exception &e) {
        lua_pushnil(L);
        lua_pushfstring(L, LUAMONGO_ERR_CALLING, LUAMONGO_GRIDFS, "store_data", e.what());
        resultcount = 2;
    }

    return resultcount;
}

/*
 * __gc
 */
static int gridfs_gc(lua_State *L) {
    GridFS *gridfs = userdata_to_gridfs(L, 1);

    delete gridfs;

    return 0;
}

/*
 * __tostring
 */
static int gridfs_tostring(lua_State *L) {
    GridFS *gridfs = userdata_to_gridfs(L, 1);

    lua_pushfstring(L, "%s: %p", LUAMONGO_GRIDFS, gridfs);

    return 1;
}

int mongo_gridfs_register(lua_State *L) {
    static const luaL_Reg gridfs_methods[] = {
        {"find_file", gridfs_find_file},
        {"list", gridfs_list},
        {"remove_file", gridfs_remove_file},
        {"store_file", gridfs_store_file},
        {"store_data", gridfs_store_data},
        {NULL, NULL}
    };

    static const luaL_Reg gridfs_class_methods[] = {
        {"New", gridfs_new},
        {NULL, NULL}
    };

    luaL_newmetatable(L, LUAMONGO_GRIDFS);
    //luaL_register(L, 0, gridfs_methods);
    luaL_setfuncs(L, gridfs_methods, 0);
    lua_pushvalue(L,-1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, gridfs_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, gridfs_tostring);
    lua_setfield(L, -2, "__tostring");
    
    lua_pop(L,1);

    #if LUA_VERSION_NUM < 502
    luaL_register(L, LUAMONGO_GRIDFS, gridfs_class_methods);
    #else
    luaL_newlib(L, gridfs_class_methods);
    #endif

    return 1;
}
