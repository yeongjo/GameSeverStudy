function plustwo(x)
local a
a = 2
return x+a
end

function add_num2(a,b)
local c
c = call_c_func_add(a, b)
return c
end

pos_x = 8
pos_y = plustwo(pos_x)