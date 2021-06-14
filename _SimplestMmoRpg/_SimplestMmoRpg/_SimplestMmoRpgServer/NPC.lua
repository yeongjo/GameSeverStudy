mId =  -1
function SetId(x)
	mId = x
	mInitX = LuaGetX(mId)
	mX = mInitX
	mInitY = LuaGetY(mId)
	mY = mInitY
end

function OnNearActorWithPlayerMove(p_id)
	p_x = LuaGetX(p_id)
	p_y = LuaGetY(p_id)
	mX = LuaGetX(mId)
	mY = LuaGetY(mId)
	--API_print(tostring(p_x)..", "..tostring(mX).."\n")

	if(p_x == mX) then
		--API_print(tostring(p_y)..", "..tostring(mY).."\n")
		if(p_y == mY) then
			LuaSendMess(p_id, mId, "HELLO")
			LuaAddEventNpcRandomMove(mId, 0000);
			LuaAddEventNpcRandomMove(mId, 1000);
			LuaAddEventNpcRandomMove(mId, 2000);
			LuaAddEventSendMess(p_id, mId, "BYE", 2000);
		end
	end
end
function OnNearActorWithSelfMove(p_id)
	
end