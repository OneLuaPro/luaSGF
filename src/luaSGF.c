/*
MIT License

Copyright (c) 2025-2026 The OneLuaPro project authors

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

#define LUASGF_VERSION "luaSGF 2.0"
#define LUASGF_METATABLE "luaSGF.Filter"

// Savitzky-Golay Filter legacy API support
typedef struct {
    float phaseAngle;
} MqsRawDataPoint_t;

// Legacy function declaration to fix -Wimplicit-function-declaration
int mes_savgolFilter(MqsRawDataPoint_t data[], size_t dataSize, uint8_t halfWindowSize,
                     MqsRawDataPoint_t filteredData[], uint8_t polynomialOrder,
                     uint8_t targetPoint, uint8_t derivativeOrder);

/**
 * Savitzky-Golay Filter Module for Lua.
 * @module luaSGF
 */

/*============================================================================
 * HELPERS
 *============================================================================*/
/**
 * @brief Helper to extract configuration fields from a Lua table.
 * Expected table format: { half_window=n, poly_order=m, ... }
 */
static void util_fill_config(lua_State *L, int index, SavgolConfig *cfg) {
  luaL_checktype(L, index, LUA_TTABLE);

  // Required fields
  lua_getfield(L, index, "half_window");
  cfg->half_window = (uint8_t)luaL_checkinteger(L, -1);
  lua_getfield(L, index, "poly_order");
  cfg->poly_order = (uint8_t)luaL_checkinteger(L, -1);

  // Optional fields with defaults
  lua_getfield(L, index, "derivative");
  cfg->derivative = (uint8_t)luaL_optinteger(L, -1, 0);
  lua_getfield(L, index, "time_step");
  cfg->time_step = (float)luaL_optnumber(L, -1, 1.0);
  lua_getfield(L, index, "boundary");
  cfg->boundary = (SavgolBoundaryMode)luaL_optinteger(L, -1,
						      SAVGOL_BOUNDARY_POLYNOMIAL);
    
  lua_pop(L, 5); // Remove the 5 fields from stack
}

/*============================================================================
 * LIFECYCLE
 *============================================================================*/
/**
 * Boundary handling modes.
 * Use these constants for the `config.boundary` field.
 * @section Boundary_Modes
 */

/** Asymmetric polynomial fit (default). 
 * @field BOUNDARY_POLYNOMIAL */
/** Mirror data at boundaries. 
 * @field BOUNDARY_REFLECT */
/** Wrap data around (for periodic signals). 
 * @field BOUNDARY_PERIODIC */
/** Extend edge values. 
 * @field BOUNDARY_CONSTANT */

/**
 * Creates a new SavgolFilter instance.
 * This constructor initializes the filter with the provided configuration and
 * precomputes the coefficients.
 * @function new
 * @tparam table config Configuration table for the filter.
 * @tparam int config.half_window Half-window size (n). The total window size is 2n+1.
 * @tparam int config.poly_order Polynomial order (m) for the fit. Must be < window size.
 * @tparam[opt=0] int config.derivative Derivative order (0 for smoothing).
 * @tparam[opt=1.0] float config.time_step Time interval between samples for scaling derivatives.
 * @tparam[opt=SAVGOL_BOUNDARY_POLYNOMIAL] int config.boundary Boundary handling mode.
 * @treturn SavgolFilter A new filter object handle.
 * @usage
 * local sg = require("luaSGF")
 * local filter = sg.new({
 *   half_window = 5,
 *   poly_order = 2,
 *   boundary = sg.BOUNDARY_REFLECT
 * })
 */
static int luaSGF_savgol_create(lua_State *L) {
  SavgolConfig config;
  util_fill_config(L, 1, &config);

  // Allocate userdata to hold the pointer to our C structure
  SavgolFilter **ud = (SavgolFilter **)lua_newuserdata(L, sizeof(SavgolFilter *));
    
  // Initialize the filter using the core library
  *ud = savgol_create(&config);
    
  if (*ud == NULL) {
    return luaL_error(L, "luaSGF.new(): invalid parameters or out of memory");
  }

  // Assign metatable for OOP-style methods and GC
  luaL_getmetatable(L, LUASGF_METATABLE);
  lua_setmetatable(L, -2);

  return 1;
}

/**
 * Frees the resources associated with the filter.
 * This method can be called manually to free memory immediately. If not called,
 * Lua's Garbage Collector will invoke it automatically when the filter object
 * is no longer referenced.
 * @function SavgolFilter:destroy
 * @usage
 * -- Manual destruction
 * filter:destroy()
 * 
 * -- After destroy, calling any method on the filter will raise an error.
 */
static int luaSGF_savgol_destroy(lua_State *L) {
  SavgolFilter **ud = (SavgolFilter **)luaL_checkudata(L, 1, LUASGF_METATABLE);
  if (*ud != NULL) {
    savgol_destroy(*ud);
    *ud = NULL; // Prevent double-free
  }
  return 0;
}

/*============================================================================
 * FILTERING
 *============================================================================*/
/**
 * Applies the filter to a table of data.
 * This method performs the filtering and returns a **new** table of the same 
 * length as the input. Boundary regions are handled according to the 
 * `boundary` mode set during construction (e.g., polynomial extrapolation, 
 * reflection, etc.).
 * 
 * @function SavgolFilter:apply
 * @tparam table data A Lua table (array-style) containing numeric values.
 * @treturn table A new table containing the filtered results.
 * @raise Error if the table is shorter than the filter window, contains holes (`nil`), 
 * or if memory allocation fails.
 * @usage
 * local data = {1.1, 2.1, 1.9, 4.2, 3.8}
 * local result = filter:apply(data)
 * assert(#result == #data)
 */
static int luaSGF_savgol_apply(lua_State *L) {
  /* 1. Retrieve and validate the filter pointer */
  /* luaL_checkudata ensures the object is of our specific metatable type */
  SavgolFilter **ud = (SavgolFilter **)luaL_checkudata(L, 1, LUASGF_METATABLE);
  luaL_argcheck(L, *ud != NULL, 1, "filter has been destroyed");

  /* 2. Ensure second argument is a table */
  luaL_checktype(L, 2, LUA_TTABLE);
    
  /* 3. Get table length using Lua 5.4 raw length */
  size_t len = lua_rawlen(L, 2);
    
  /* 4. Check against filter window size */
  if (len < (size_t)(*ud)->window_size) {
    return luaL_error(L, "input table too short (min: %d, got: %d)", 
		      (*ud)->window_size, (int)len);
  }
  
  /* 5. Allocate memory (using malloc for large data chunks) */
  float *in_data  = (float *)malloc(len * sizeof(float));
  float *out_data = (float *)malloc(len * sizeof(float));
    
  if (!in_data || !out_data) {
    free(in_data); 
    free(out_data);
    return luaL_error(L, "memory allocation failed");
  }

  /* 6. Extract values from Lua table */
  /* In Lua 5.4, we use lua_rawgeti which is fast and avoids __index metamethods */
  for (size_t i = 1; i <= len; i++) {
    if (lua_rawgeti(L, 2, i) == LUA_TNIL) {
      free(in_data); free(out_data);
      return luaL_error(L, "input table has a hole at index %d", (int)i);
    }
    in_data[i-1] = (float)luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }

  /* 7. Core calculation */
  if (savgol_apply(*ud, in_data, out_data, len) != 0) {
    free(in_data); free(out_data);
    return luaL_error(L, "savgol_apply failed");
  }

  /* 8. Create result table */
  /* Pre-allocating the array part to 'len' prevents rehashes */
  lua_createtable(L, (int)len, 0);
    
  for (size_t i = 0; i < len; i++) {
    /* Convert float back to lua_Number (usually double) */
    lua_pushnumber(L, (lua_Number)out_data[i]);
    /* Set table[i+1] = value */
    lua_rawseti(L, -2, i + 1);
  }

  /* 9. Final Cleanup */
  free(in_data);
  free(out_data);
  
  return 1; /* One return value: the new table */
}

/**
 * Applies the filter returning only VALID output.
 * This method does not perform boundary extrapolation. It returns only those 
 * samples where the filter window was fully contained within the input data. 
 * As a result, the output table is shorter than the input table.
 *
 * **Output length** = `input_length - 2 * half_window`.
 *
 * @function SavgolFilter:apply_valid
 * @tparam table data Array-style table containing numeric values to be filtered.
 * @treturn table A new (shorter) table containing the valid filtered samples.
 * @raise Error if the input table is shorter than the filter window size or if 
 * memory allocation fails.
 * @usage
 * -- If half_window is 5, the first and last 5 samples are discarded.
 * local input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11} -- length 11
 * local result = filter:apply_valid(input)
 * print(#result) -- Output: 1
 */
static int luaSGF_savgol_apply_valid(lua_State *L) {
  /* Retrieve filter pointer */
  SavgolFilter **ud = (SavgolFilter **)luaL_checkudata(L, 1, LUASGF_METATABLE);
  luaL_argcheck(L, *ud != NULL, 1, "filter has been destroyed");

  /* Ensure argument 2 is the input data table */
  luaL_checktype(L, 2, LUA_TTABLE);
  size_t in_len = lua_rawlen(L, 2);
    
  /* Calculate expected output length: L - 2*n */
  int hw = (*ud)->config.half_window;
    
  /* Boundary check: input must be at least one full window size */
  if (in_len < (size_t)(*ud)->window_size) {
    return luaL_error(L, "input table too short for 'valid' output (min: %d, got: %d)", 
		      (*ud)->window_size, (int)in_len);
  }
    
  size_t out_len = in_len - (2 * (size_t)hw);

  /* Allocate memory for C arrays */
  float *in_data  = (float *)malloc(in_len * sizeof(float));
  float *out_data = (float *)malloc(out_len * sizeof(float));
    
  if (!in_data || !out_data) {
    free(in_data); free(out_data);
    return luaL_error(L, "memory allocation failed");
  }

  /* Step 1: Copy from Lua table to C input array */
  for (size_t i = 1; i <= in_len; i++) {
    lua_rawgeti(L, 2, (int)i);
    in_data[i-1] = (float)luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
  
  /* Step 2: Execute core library function */
  /* According to header: returns samples written, or 0 on error */
  size_t written = savgol_apply_valid(*ud, in_data, in_len, out_data);
  
  if (written == 0 && out_len > 0) {
    free(in_data); free(out_data);
    return luaL_error(L, "savgol_apply_valid core execution failed");
  }
  
  /* Step 3: Create the shorter Lua table and populate with results */
  lua_createtable(L, (int)written, 0);
  for (size_t i = 0; i < written; i++) {
    lua_pushnumber(L, (lua_Number)out_data[i]);
    lua_rawseti(L, -2, (int)(i + 1));
  }

  /* Cleanup */
  free(in_data);
  free(out_data);

  return 1; /* Return the result table */
}

/**
 * Direct filter calculation (Legacy API).
 * This function can be called as `sg.calc(...)` or directly as `sg(...)`.
 * It creates a temporary filter, processes the data, and returns the result.
 * 
 * @function calc
 * @tparam int half_window Half-window size (n).
 * @tparam int poly_order Polynomial order (m).
 * @tparam int target_point Target point within the window (0 for center).
 * @tparam int derivative Derivative order (0 for smoothing).
 * @tparam table data Input signal as a Lua table (1-based indexing).
 * @treturn[1] table Filtered data as a new Lua table.
 * @treturn[1] nil If successful, the second return value is nil.
 * @treturn[2] nil If an error occurs, the first return value is nil.
 * @treturn[2] string An error message describing the failure.
 * @usage
 * local sg = require("luaSGF")
 * -- Using the explicit function:
 * local result, err = sg.calc(5, 2, 0, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11})
 * 
 * -- Using the functional style (shorthand):
 * local result, err = sg(5, 2, 0, 0, my_data_table)
 */
static int luaSGF_calc(lua_State *L) {
  /* 
   * 1. Handle argument offset
   * If called as sg(hw, poly, ...), index 1 is the table 'sg'.
   * If called as sg.calc(hw, poly, ...), index 1 is 'hw'.
   */
  int offset = (lua_istable(L, 1) && lua_gettop(L) > 1) ? 1 : 0;

  /* 2. Extract and validate arguments using the offset */
  uint8_t halfWindowSize  = (uint8_t)luaL_checkinteger(L, 1 + offset);
  uint8_t polynomialOrder = (uint8_t)luaL_checkinteger(L, 2 + offset);
  uint8_t targetPoint     = (uint8_t)luaL_checkinteger(L, 3 + offset);
  uint8_t derivativeOrder = (uint8_t)luaL_checkinteger(L, 4 + offset);
  
  /* Basic validation against your rules */
  if (halfWindowSize < 1) {
    lua_pushnil(L);
    lua_pushstring(L, "Half-window size must be greater than 0.");
    return 2;
  }
  if (polynomialOrder >= (uint8_t)(2 * halfWindowSize + 1)) {
    lua_pushnil(L);
    lua_pushstring(L, "Polynomial order must be less than the filter window size.");
    return 2;
  }
  if (targetPoint > (2 * halfWindowSize)) {
    lua_pushnil(L);
    lua_pushstring(L, "Target point must be within the filter window.");
    return 2;
  }

  /* 3. Validate input table (now at index 5 + offset) */
  luaL_checktype(L, 5 + offset, LUA_TTABLE);
  size_t dataSize = (size_t)lua_rawlen(L, 5 + offset);
  if ((size_t)(2 * halfWindowSize + 1) > dataSize) {
    lua_pushnil(L);
    lua_pushstring(L, "Filter window size must not exceed data size.");
    return 2;
  }

  /* 4. Allocate memory */
  MqsRawDataPoint_t *rawData = (MqsRawDataPoint_t*)malloc(dataSize * sizeof(MqsRawDataPoint_t));
  MqsRawDataPoint_t *filteredData = (MqsRawDataPoint_t*)malloc(dataSize * sizeof(MqsRawDataPoint_t));
    
  if (!rawData || !filteredData) {
    free(rawData); free(filteredData);
    lua_pushnil(L);
    lua_pushstring(L, "Memory allocation failure.");
    return 2;
  }

  /* 5. Transfer Lua table to C array (using offset for the table index) */
  for (size_t i = 1; i <= dataSize; i++) {
    lua_rawgeti(L, 5 + offset, (int)i);
    rawData[i-1].phaseAngle = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
  }

  /* 6. Perform calculation */
  int res = mes_savgolFilter(rawData, dataSize, halfWindowSize, filteredData,
                               polynomialOrder, targetPoint, derivativeOrder);
    
  free(rawData);

  if (res != 0) {
    free(filteredData);
    lua_pushnil(L);
    lua_pushstring(L, "Internal filter execution failed.");
    return 2;
  }

  /* 7. Build result table (Lua 5.4 optimized) */
  lua_createtable(L, (int)dataSize, 0);
  for (size_t i = 0; i < dataSize; i++) {
    lua_pushnumber(L, (lua_Number)filteredData[i].phaseAngle);
    lua_rawseti(L, -2, (int)(i + 1));
  }

  free(filteredData);
  lua_pushnil(L); // No error message
  return 2;
}

/*============================================================================
 * REGISTRATION
 *============================================================================*/

static const struct luaL_Reg luaSGF_filter_methods[] = {
  {"__gc",    luaSGF_savgol_destroy},
  {"destroy", luaSGF_savgol_destroy},
  {"apply",   luaSGF_savgol_apply},
  {"apply_valid", luaSGF_savgol_apply_valid},
  {NULL, NULL}
};

static const struct luaL_Reg luaSGF_funcs[] = {
  {"new", luaSGF_savgol_create},
  {"calc", luaSGF_calc}, // Legacy direct call
  {NULL, NULL}
};

/**
 * @brief Main entry point for require("luaSGF")
 */
LUALIB_API int luaopen_luaSGF(lua_State *L) {
  // Create and setup the metatable for our userdata
  luaL_newmetatable(L, LUASGF_METATABLE);
  lua_pushvalue(L, -1);            // Duplicate metatable
  lua_setfield(L, -2, "__index");  // MT.__index = MT for method lookup
  luaL_setfuncs(L, luaSGF_filter_methods, 0);
  lua_pop(L, 1);                   // Pop metatable from stack

  // Create the library table
  luaL_newlib(L, luaSGF_funcs);

  // Create a metatable for the library table itself to support __call
  lua_newtable(L); 
  lua_pushcfunction(L, luaSGF_calc);
  lua_setfield(L, -2, "__call");
  lua_setmetatable(L, -2);
  
  // Register constants/enums
  lua_pushinteger(L, SAVGOL_BOUNDARY_POLYNOMIAL);
  lua_setfield(L, -2, "BOUNDARY_POLYNOMIAL");
  lua_pushinteger(L, SAVGOL_BOUNDARY_REFLECT);
  lua_setfield(L, -2, "BOUNDARY_REFLECT");
  lua_pushinteger(L, SAVGOL_BOUNDARY_PERIODIC);
  lua_setfield(L, -2, "BOUNDARY_PERIODIC");
  lua_pushinteger(L, SAVGOL_BOUNDARY_CONSTANT);
  lua_setfield(L, -2, "BOUNDARY_CONSTANT");

  // Set version info
  lua_pushliteral(L, LUASGF_VERSION);
  lua_setfield(L, -2, "_VERSION");
    
  return 1;
}
