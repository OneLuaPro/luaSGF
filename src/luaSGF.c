/*
MIT License

Copyright (c) 2025 Kritzel Kratzel for OneLuaPro

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <stdint.h>
#include <savgolFilter.h>

#define LUASGF_VERSION "luaSGF 1.0"

static int luaSGF_calc(lua_State *L) {
  // table, errmsg = calc(<halfWindowSize>,<polynomialOrder>,
  //                      <targetPoint>,<derivativeOrder>,<table>)
  // tables with "natural" Lua-indexing 1,2,3,4,5,...,N.
  // On success: returns <filteredData>, nil
  // On failire: returns nil, <error_message>

  // Check arguments
  uint8_t halfWindowSize = (uint8_t)luaL_checkinteger(L,1);
  if (halfWindowSize < 1) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Half-window size must be greater than 0.");
    return 2;
  }
  uint8_t polynomialOrder = (uint8_t)luaL_checkinteger(L,2);
  if (polynomialOrder < 0) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Polynomial order must be a positive integer.");
    return 2;
  }
  if (polynomialOrder >= (2 * halfWindowSize + 1)) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Polynomial order must be less than the filter window size.");
    return 2;
  }
  uint8_t targetPoint = (uint8_t)luaL_checkinteger(L,3);
  if (targetPoint > (2 * halfWindowSize)) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Target point must be within the filter window.");
    return 2;
  }
  uint8_t derivativeOrder = (uint8_t)luaL_checkinteger(L,4);
  if (derivativeOrder < 0) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Derivative order must be a positive integer.");
    return 2;
  }
  if (lua_istable(L,5) == 0) {
    // input data is not a table
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Input data is not a Lua-table.");
    return 2;
  }
  // Get table length (is dataSize)
  size_t dataSize = (size_t)lua_rawlen(L, 5);	// get table length
  if ((2 * halfWindowSize + 1) > dataSize) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Filter window size must not exceed data size.");
    return 2;
  }
  // allocate spaces
  MqsRawDataPoint_t *rawData = (MqsRawDataPoint_t*)malloc(dataSize*sizeof(MqsRawDataPoint_t));
  MqsRawDataPoint_t *filteredData = (MqsRawDataPoint_t*)malloc(dataSize*sizeof(MqsRawDataPoint_t));
  if (rawData == NULL || filteredData == NULL) {
    lua_pushnil(L);	// no result
    lua_pushstring(L,"Could not allocate memory for rawData and filteredData.");
    return 2;
  }
  // read table and transfer to dynamic array
  lua_pushnil(L);
  while (lua_next(L,5) != 0) {
    // key at index -2
    int key = (int)lua_tointeger(L,-2);
    // value at index -1
    double value = (double)lua_tonumber(L,-1);
    // pop the value, keep the key for the next iteration
    lua_pop(L,1);
    rawData[key-1].phaseAngle = value;
  }
  // calculate
  mes_savgolFilter(rawData, dataSize, halfWindowSize, filteredData,
		   polynomialOrder, targetPoint, derivativeOrder);
  free(rawData);
  // build result table on stack
  lua_newtable(L);
  int index = lua_gettop(L);
  for (int i=0; i < dataSize; i++) {
    // key
    lua_pushinteger(L,i+1);
    // value
    lua_pushnumber(L,(double)filteredData[i].phaseAngle);
    // set table[key] = value (pops the key and the value from the stack)
    lua_settable(L,index);
  }
  free(filteredData);
  lua_pushnil(L);		// no error msg
  return 2;
}

static const struct luaL_Reg luaSGF_metamethods [] = {
  {"__call", luaSGF_calc},
  {NULL, NULL}
};

static const struct luaL_Reg luaSGF_funcs [] = {
  {"calc", luaSGF_calc},
  {NULL, NULL}
};

LUALIB_API int luaopen_luaSGF(lua_State *L){
  luaL_newlib(L, luaSGF_funcs);
  luaL_newlib(L, luaSGF_metamethods);
  lua_setmetatable(L, -2);
  lua_pushliteral(L,LUASGF_VERSION);
  lua_setfield(L,-2,"_VERSION");
  return 1;
}
