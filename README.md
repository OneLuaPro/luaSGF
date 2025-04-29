# luaSGF
Lua bindings for the [Savitzky-Golay-Filter Implementation](https://github.com/OneLuaPro/Savitzky-Golay-Filter) for data filtering or smoothing.

## Function Reference

### calc()

Launches the data filtering process on an input data table ***rawData*** with length $N$ using the following parameters:

- ***halfWindowSize***: Defines the filter window width. Actual filter window has a size of $2\cdot halfWindowSize+1$. Valid range is $1 \le halfWindowSize \le \frac{N-1}{2}$.
- ***polynomialOrder***: The order of the polynomial used to fit the samples. Valid range is  $0 \le polynomialOrder \lt 2\cdot halfWindowSize+1$.
- ***targetPoint***: Valid range is $0 \le targetPoint \le 2\cdot halfWindowSize$.
  - *targetPoint = 0*: The filter smooths data based on both future and past values. This setting is more suited for non-real-time applications where all data points are available.
  - *targetPoint = halfWindowSize*: The filter smooths data based on only the present and past data, making it suitable for real-time applications or causal filtering.

- ***derivativeOrder***: The order of the derivative to compute. This must be a nonnegative integer. The default is 0, which means to filter the data without differentiating.
- ***rawData***: Table (with "natural" Lua-indexing 1,2,3,4,5,...,N) containing the numbers to be smoothed.

```Lua
local sgf = require("luaSGF")

local result, errmsg = sgf.calc(halfWindowSize, polynomialOrder, targetPoint, derivativeOrder, rawData)

-- On success:
result = <RESULT_TABLE>	-- Lua-table with smoothed data
errmsg = nil	-- no error message
-- On failure:
result = nil	-- no info available
errmsg = "Detailed error description"
```

**Remark**: The current filter implementation uses data caching for the computation of the Gram polynomials. You can benefit from this significant speed-up, provided the following limitations are met:

- $halfWindowSize \le 32$
- $polynomialOrder \le 4$
- $derivativeOrder \le 4$

Gram polynomials exceeding these limits will be calculated iteratively on request and without using the data cache.

### ._VERSION()

Displays version info. Example:

```Lua
Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio
> sgf = require("luaSGF")
> sgf._VERSION
luaSGF 1.0
```

## Further Reading

- https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter
- https://github.com/Tugbars/Savitzky-Golay-Filter
- https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.savgol_filter.html

## License

See https://github.com/OneLuaPro/luaSGF/blob/master/LICENSE.
