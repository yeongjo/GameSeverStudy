mId =  -1
mX = -1
mY = -1
function SetId(x)
	mId = x
	mInitX = LuaGetX(mId)
	mX = mInitX
	mInitY = LuaGetY(mId)
	mY = mInitY
end

function OnNearActorWithPlayerMove(playerId)
	playerX = LuaGetX(playerId)
	playerY = LuaGetY(playerId)
	--API_print(tostring(playerX)..", "..tostring(mX).."\n")

	if(playerX == mX and playerY == mY) then
		LuaSendMess(playerId, mId, "HELLO")
		LuaAddEventSendMess(playerId, mId, "BYE", 1000);
	end
end