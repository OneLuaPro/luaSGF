# luaSGF
Lua bindings for the [Savitzky-Golay-Filter Implementation](https://github.com/OneLuaPro/Savitzky-Golay-Filter) for data filtering or smoothing.

## Function Reference

This section describes the modern object-oriented API. Using this approach is highly recommended as it allows you to reuse precomputed filter weights for multiple data sets, significantly improving performance.

### `new()`

Creates a new filter object. This is the recommended way to use the library as it precomputes filter weights for better performance.

```lua
local sgf = require("luaSGF")

local config = {
    half_window = 5,                -- n: spans 2n+1 points (Max: 32)
    poly_order = 2,                 -- m: polynomial order (Max: 10)
    derivative = 0,                 -- d: derivative order (Max: 4)
    time_step = 1.0,                -- Δt: for scaling derivatives (Default: 1.0)
    boundary = sgf.BOUNDARY_REFLECT -- boundary mode (Default: POLYNOMIAL)
}

local filter = sgf.new(config)
```

**Boundary Modes**:

- `sgf.BOUNDARY_POLYNOMIAL`: Asymmetric polynomial fit (default)
- `sgf.BOUNDARY_REFLECT`: Mirror data at boundaries
- `sgf.BOUNDARY_PERIODIC`: Wrap data around (for periodic signals)
- `sgf.BOUNDARY_CONSTANT`: Extend edge values

## Filter Methods

Once a filter object is created via `new()`, the following methods are available. The following description assumes the filter has been created with `local filter = sgf.new(config)` upfront.

### `filter:apply(data)`

Applies the filter to the input table. Returns a **new** table of the same length. Boundary regions are handled according to the filter's configuration.

```lua
local smoothed = filter:apply({1, 2, 3, 2, 1})
```

### `filter:apply_valid(data)`

Applies the filter but returns only the "valid" part where the window fully fits the data. No boundary extrapolation is performed.
**Result length** = `input_length - 2 * half_window`.

```lua
local valid_data = filter:apply_valid(input_table)
```

### `filter:destroy()`

Manually destroys the filter and frees C memory. Note: This is also handled automatically by the Lua Garbage Collector.

## Legacy Function Reference

### `calc() / __call()`

For backward compatibility, the module can be called directly or via `.calc()`. This creates a temporary filter for a single use. Launches the data filtering process on an input data table ***rawData*** with length $N$ using the following parameters:

- ***halfWindowSize***: Defines the filter window width. Actual filter window has a size of $2\cdot halfWindowSize+1$. Valid range is $1 \le halfWindowSize \le 32$.
- ***polynomialOrder***: The order of the polynomial used to fit the samples. Valid range is  $0 \le polynomialOrder \le 10$ (must be $$\lt 2 \cdot halfWindowSize + 1$$.)
- ***derivativeOrder***: The order of the derivative to compute. Valid range is $$0 \le derivativeOrder \le 4$$. The default is 0, which means to filter the data without differentiating.
- ***targetPoint***: Valid range is $0 \le targetPoint \le 2\cdot halfWindowSize$.
  - *targetPoint = 0*: The filter smooths data based on both future and past values. This setting is more suited for non-real-time applications where all data points are available.
  - *targetPoint = halfWindowSize*: The filter smooths data based on only the present and past data, making it suitable for real-time applications or causal filtering.
- ***rawData***: Table (with "natural" Lua 1-based indexing) containing the numbers to be smoothed.

```Lua
local sgf = require("luaSGF")

-- Signature 1
local result, errmsg = sgf.calc(halfWindowSize, polynomialOrder, targetPoint, derivativeOrder, rawData)
-- Signature 2
local result, errmsg = sgf(halfWindowSize, polynomialOrder, targetPoint, derivativeOrder, rawData)

-- On success:
result = <RESULT_TABLE>	-- Lua-table with smoothed data
errmsg = nil	-- no error message
-- On failure:
result = nil	-- no info available
errmsg = "Detailed error description"
```

### `._VERSION()`

Displays version info. Example:

```Lua
Lua 5.4.8  Copyright (C) 1994-2024 Lua.org, PUC-Rio
> sgf = require("luaSGF")
> sgf._VERSION
luaSGF 2.0
```

## Further Reading

- https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter
- https://github.com/Tugbars/Savitzky-Golay-Filter
- https://www.mathworks.com/help/signal/ref/sgolayfilt.html
- https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.savgol_filter.html

## License

See https://github.com/OneLuaPro/luaSGF/blob/master/LICENSE.
