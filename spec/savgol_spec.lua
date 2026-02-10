local sg = require("luaSGF")

describe("SavgolFilter Lifecycle and Validation", function()

    it("Create filter with valid config", function()
        local cfg = {
            half_window = 5,
            poly_order = 2,
            derivative = 0,
            time_step = 1.0,
            boundary = sg.BOUNDARY_POLYNOMIAL
        }
        
        local filter = sg.new(cfg)
        
        assert.is_not_nil(filter)
        assert.is_userdata(filter)
        
        -- Cleanup
        filter:destroy()
    end)

    it("Destroy NULL filter (should not crash)", function()
        -- In Lua, we can simulate this by calling destroy twice.
        -- The first call sets the internal C pointer to NULL, 
        -- the second call handles the NULL pointer.
        local filter = sg.new({half_window = 5, poly_order = 2})
        
        filter:destroy() -- Internal pointer becomes NULL
        
        assert.has_no.errors(function()
            filter:destroy() -- Should be a safe no-op
        end)
    end)

    it("Reject invalid config: half_window = 0", function()
        local cfg = {
            half_window = 0,
            poly_order = 2
        }
        
        -- Since luaSGF_savgol_create calls luaL_error if savgol_create returns NULL,
        -- we expect an error here.
        assert.has_error(function()
            sg.new(cfg)
        end)
    end)

    it("Reject invalid config: poly_order >= window_size", function()
        -- half_window=2 -> window_size=5, so poly_order 10 is invalid
        local cfg = {
            half_window = 2,
            poly_order = 10
        }
        
        assert.has_error(function()
            sg.new(cfg)
        end)
    end)

    it("Reject invalid config: derivative > poly_order", function()
        local cfg = {
            half_window = 5,
            poly_order = 2,
            derivative = 3
        }
        
        assert.has_error(function()
            sg.new(cfg)
        end)
    end)

end)

describe("SavgolFilter Mathematical Properties", function()

    -- Helper: Creates a unit impulse signal (all 0, center is 1)
    local function unit_impulse(half_window)
        local size = 2 * half_window + 1
        local data = {}
        for i = 1, size do data[i] = 0.0 end
        data[half_window + 1] = 1.0
        return data
    end

    it("Smoothing weights sum to 1", function()
        local cfg = { half_window = 5, poly_order = 3, derivative = 0 }
        local filter = sg.new(cfg)
        
        -- Apply filter to a constant signal of 1.0s
        -- If weights sum to 1, the result must be 1.0
        local input = {}
        for i = 1, 20 do input[i] = 1.0 end
        
        local result = filter:apply(input)
        
        -- Check center value (index 10 is far enough from edges)
        assert.near(1.0, result[10], 1e-5)
    end)

    it("Smoothing weights are symmetric", function()
        local cfg = { half_window = 5, poly_order = 3, derivative = 0 }
        local filter = sg.new(cfg)
        
        -- We use a unit impulse to "extract" the weights
        local input = unit_impulse(5)
        local result = filter:apply(input)
        
        -- In a symmetric filter, the response to an impulse at the center 
        -- must be symmetric around that center.
        -- result[1] is the left edge, result[11] is the right edge
        assert.near(result[1], result[11], 1e-6)
        assert.near(result[2], result[10], 1e-6)
    end)

    it("First derivative weights are antisymmetric", function()
        local hw = 5
        local cfg = { 
            half_window = hw, 
            poly_order = 3, 
            derivative = 1, 
            time_step = 1.0 
        }
        local filter = sg.new(cfg)
        
        -- We create a longer signal so the filter can "slide" over the impulse
        -- without hitting the boundaries immediately.
        local input = {}
        for i = 1, 30 do input[i] = 0.0 end
        input[15] = 1.0  -- Impulse at index 15
        
        local result = filter:apply(input)
        
        -- When the filter is centered on the impulse (at index 15), 
        -- it sees the center weight.
        -- When it is at index 14, it sees the weight to the right of center.
        -- When it is at index 16, it sees the weight to the left of center.
        
        -- Center must be 0
        assert.near(0.0, result[15], 1e-6)
        
        -- Values around center must be antisymmetric: result[15-k] == -result[15+k]
        -- Let's check the neighbors:
        assert.near(result[14], -result[16], 1e-6)
        assert.near(result[13], -result[17], 1e-6)
        assert.near(result[10], -result[20], 1e-6)
        
        -- Additional check: the value at result[14] must NOT be 0
        assert.is_not.near(0.0, result[14], 1e-6)
    end)

local sg = require("luaSGF")

describe("SavgolFilter Signal Application", function()

    it("Apply to constant signal: output unchanged", function()
        -- Equivalent to test_apply_constant
        local cfg = { half_window = 5, poly_order = 2, derivative = 0 }
        local filter = sg.new(cfg)
        
        local input = {}
        for i = 1, 50 do input[i] = 42.0 end
        
        -- In Lua, apply returns a new table (no need to check result == 0)
        local result = filter:apply(input)
        
        assert.is.equal(#input, #result)
        for i = 1, #result do
            assert.near(42.0, result[i], 0.01)
        end
    end)

    it("Apply to linear signal: interior preserved", function()
        -- Equivalent to test_apply_linear (y = 3x + 7)
        local cfg = { half_window = 5, poly_order = 2, derivative = 0 }
        local filter = sg.new(cfg)
        
        local input = {}
        for i = 1, 50 do 
            input[i] = 3.0 * i + 7.0 
        end
        
        local result = filter:apply(input)
        
        -- Check interior (indices 11 to 40, avoiding 5-sample edges)
        for i = 11, 40 do
            local expected = 3.0 * i + 7.0
            assert.near(expected, result[i], 0.01)
        end
    end)

    it("First derivative of y=3x equals 3", function()
        -- Equivalent to test_derivative_linear
        local cfg = { 
            half_window = 5, 
            poly_order = 2, 
            derivative = 1, 
            time_step = 1.0 
        }
        local filter = sg.new(cfg)
        
        local input = {}
        for i = 1, 50 do input[i] = 3.0 * i end
        
        local result = filter:apply(input)
        
        -- For a linear ramp with slope 3, derivative should be 3.0
        for i = 11, 40 do
            assert.near(3.0, result[i], 0.01)
        end
    end)

end)

describe("SavgolFilter Boundary Modes", function()
    -- Helper to create a constant signal
    local function constant_signal(len, val)
        local t = {}
        for i = 1, len do t[i] = val end
        return t
    end

    local input_val = 42.0
    local signal = constant_signal(50, input_val)

    it("REFLECT mode: constant signal unchanged", function()
        local filter = sg.new({
            half_window = 5,
            poly_order = 2,
            boundary = sg.BOUNDARY_REFLECT
        })
        
        local result = filter:apply(signal)
        
        for i = 1, #result do
            assert.near(input_val, result[i], 0.01)
        end
    end)

    it("PERIODIC mode: constant signal unchanged", function()
        local filter = sg.new({
            half_window = 5,
            poly_order = 2,
            boundary = sg.BOUNDARY_PERIODIC
        })
        
        local result = filter:apply(signal)
        
        for i = 1, #result do
            assert.near(input_val, result[i], 0.01)
        end
    end)

    it("CONSTANT mode: constant signal unchanged", function()
        local filter = sg.new({
            half_window = 5,
            poly_order = 2,
            boundary = sg.BOUNDARY_CONSTANT
        })
        
        local result = filter:apply(signal)
        
        for i = 1, #result do
            assert.near(input_val, result[i], 0.01)
        end
    end)
end)

describe("SavgolFilter Valid Mode", function()

    it("savgol_apply_valid: correct output length", function()
        -- Equivalent to test_valid_mode_length
        local hw = 5
        local cfg = { half_window = hw, poly_order = 2, derivative = 0 }
        local filter = sg.new(cfg)
        
        local input = {}
        for i = 1, 100 do input[i] = i - 1 end -- 0 to 99
        
        local result = filter:apply_valid(input)
        
        -- Expected length: 100 - (2 * 5) = 90
        local expected_len = 100 - 2 * hw
        assert.is.equal(expected_len, #result)
    end)

    it("savgol_apply_valid: linear signal preserved", function()
        -- Equivalent to test_valid_mode_linear
        local hw = 5
        local cfg = { half_window = hw, poly_order = 2, derivative = 0 }
        local filter = sg.new(cfg)
        
        local input = {}
        for i = 1, 100 do input[i] = i - 1 end -- Linear signal: f(i) = i-1
        
        local result = filter:apply_valid(input)
        
        -- In 'valid' mode, the first output corresponds to input[hw + 1]
        -- input[6] is 5.0, so result[1] should be 5.0
        for i = 1, #result do
            local expected = (i - 1) + hw
            assert.near(expected, result[i], 0.1)
        end
    end)

end)

describe("SavgolFilter Noise Reduction", function()

    it("Noise reduction: output error < input error", function()
        -- Equivalent to test_noise_reduction
        local hw = 10
        local filter = sg.new({half_window = hw, poly_order = 3, derivative = 0})
        
        local len = 200
        local input = {}
        local true_signal = {}
        
        -- Seed the random generator for reproducibility
        math.randomseed(12345)
        
        -- Create noisy sine wave
        for i = 1, len do
            -- i is 1-based, we map to x starting from 0
            local x = i - 1
            local signal = math.sin(2.0 * math.pi * x / 50.0)
            local noise = 0.2 * (math.random() - 0.5)
            
            true_signal[i] = signal
            input[i] = signal + noise
        end
        
        local result = filter:apply(input)
        
        -- Compute RMS error vs true signal (avoiding edges: index 21 to 180)
        local input_err_sq_sum = 0.0
        local output_err_sq_sum = 0.0
        local count = 0
        
        for i = 21, 180 do
            local in_diff = input[i] - true_signal[i]
            local out_diff = result[i] - true_signal[i]
            
            input_err_sq_sum = input_err_sq_sum + (in_diff * in_diff)
            output_err_sq_sum = output_err_sq_sum + (out_diff * out_diff)
            count = count + 1
        end
        
        local input_rms = math.sqrt(input_err_sq_sum / count)
        local output_rms = math.sqrt(output_err_sq_sum / count)
        
        -- Verify that the filtered signal is closer to the true sine wave
        assert.is_true(output_rms < input_rms)
        
        -- Optional: print the improvement for information
        print(string.format("\nRMS Error - Input: %.4f, Output: %.4f", input_rms, output_rms))
    end)

end)

describe("luaSGF Legacy API Compatibility", function()

    local hw, poly, deriv = 5, 2, 0
    local input = {}
    for i = 1, 50 do input[i] = math.sin(i/10) end

    it("calc() produces identical results to new Object API", function()
        -- 1. Modern API
        local filter = sg.new({
            half_window = hw,
            poly_order = poly,
            derivative = deriv
        })
        local result_modern = filter:apply(input)

        -- 2. Legacy API via sg.calc()
        -- Signature: calc(hw, poly, target, deriv, table)
        local result_legacy, err = sg.calc(hw, poly, 0, deriv, input)
        
        assert.is_nil(err)
        assert.is_table(result_legacy)
        assert.is.equal(#result_modern, #result_legacy)

        -- Compare values
        for i = 1, #result_modern do
            assert.near(result_modern[i], result_legacy[i], 1e-6)
        end
    end)

    it("__call (functional style) works correctly", function()
        -- Testing the sg(...) shorthand
        local result, err = sg(hw, poly, 0, deriv, input)
        
        assert.is_nil(err)
        assert.is_table(result)
        assert.is.equal(#input, #result)
    end)

    it("calc() returns nil and error message on invalid input", function()
        -- Test invalid half_window (0)
        local result, err = sg.calc(0, 2, 0, 0, input)
        
        assert.is_nil(result)
        assert.is.equal("Half-window size must be greater than 0.", err)

        -- Test poly_order >= window_size
        local result2, err2 = sg.calc(2, 10, 0, 0, input)
        
        assert.is_nil(result2)
        assert.is_string(err2)
    end)

    it("calc() handles target point validation", function()
        -- targetPoint must be <= 2*half_window
        -- hw=5 -> max target=10. We try 11.
        local result, err = sg.calc(5, 2, 11, 0, input)
        
        assert.is_nil(result)
        assert.is.equal("Target point must be within the filter window.", err)
    end)

end)

end)

