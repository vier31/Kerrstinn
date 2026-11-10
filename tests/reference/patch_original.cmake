# Copies original/engineWorkers.cpp into the build tree with the two changes
# that make it portable to arm64, matching the port's documented deviations:
#   abs((int) x): x86 gives INT_MIN for NaN, arm64 gives 0  -> ref_abs_trunc()
#   PiiTiiVii at amount 1.0 produces NaN                      -> 0
# Fails if the original text is not found, so the patch can't silently rot.
file (READ "${SRC}" text)

function (patch from to)
    string (FIND "${text}" "${from}" at)
    if (at EQUAL -1)
        message (FATAL_ERROR "patch_original: '${from}' not found in ${SRC}")
    endif ()
    string (REPLACE "${from}" "${to}" text "${text}")
    set (text "${text}" PARENT_SCOPE)
endfunction ()

patch ("abs((int)(inMY))" "ref_abs_trunc(inMY)")
patch ("out[i] = inMY;" "out[i] = (inMY != inMY) ? 0.0f : inMY;")

file (WRITE "${DST}" "int ref_abs_trunc(float);\n#line 1 \"engineWorkers.cpp\"\n${text}")
