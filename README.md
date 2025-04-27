# luaSGF
Lua bindings for the [Savitzky-Golay-Filter Implementation](https://github.com/OneLuaPro/Savitzky-Golay-Filter).

## Function Reference

### calc()

Launches calculation. Example:

```Lua
local sgf = require("luaSGF")

-- halfWindowSize, polynomialOrder, targetPoint, derivativeOrder
--   see https://github.com/OneLuaPro/Savitzky-Golay-Filter
-- rawData: table of input data with "natural" Lua-indexing 1,2,3,4,5,...,N.
local result, errmsg = sgf.calc(halfWindowSize, polynomialOrder, targetPoint, derivativeOrder, rawData)

-- On success:
result = <RESULT_TABLE>	-- Lua-table with smoothed data
errmsg = nil	-- no error message
-- On failure:
result = nil	-- no info available
errmsg = "Detailed error description"
```

### ._VERSION()

Displays version info. Example:

```Lua
Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio
> sgf = require("luaSGF")
> sgf._VERSION
luaSGF 1.0
```

## License

See https://github.com/OneLuaPro/luaSGF/blob/master/LICENSE.
