mId =  -1
mInitX = -1
mInitY = -1
mAgroDistance = 0
mHp = 0
mLevel = 0
mExp = 0
mDamage = 0
mIsInitialized = false
EFindPlayerAct_Peace = 0
EFindPlayerAct_Agro = 1
mFindPlayerAct = EFindPlayerAct_Peace
ESoloMove_Fixing = 0
ESoloMove_Roaming = 1
mSoloMove = ESoloMove_Fixing
mTargetActorId = -1

function SetId(x)
	mId = x
	mInitX = LuaGetX(mId)
	mInitY = LuaGetY(mId)
	InitStat()
end

function Reset()
	mX = mInitX
	mY = mInitY
	LuaSetPos(mId, mX, mY)
	InitStat()
end

function InitStat()
	if(mFindPlayerAct == EFindPlayerAct_Agro) then
		mAgroDistance = 11
	end
	mHp = 2
	mLevel = 1
	mExp = mLevel*mLevel*2
	mDamage = 1
	mTargetActorId = -1
end

function Distance(x1, y1, x2, y2)
	x = x1 - x2
	y = y1 - y2
	return math.sqrt(x*x+y*y)
end

function RandomMove()
	randomDir = math.random(4)
	if(randomDir == 1) then
		mX = mX+1
	elseif(randomDir == 2) then
		mX = mX-1
	elseif(randomDir == 3) then
		mY = mY+1
	else
		mY = mY-1
	end
	LuaSetPos(mId, mX, mY)
end

function Tick(actorIdArray)
	if(mFindPlayerAct == EFindPlayerAct_Peace) then
		if(mTargetActorId ~= -1) then
			targetActorX = LuaGetX(mTargetActorId)
			targetActorY = LuaGetY(mTargetActorId)
			LuaPrint(mTargetActorId..": "..targetActorX..","..targetActorY.."\n")
		else
			if(ESoloMove_Roaming == mSoloMove) then
				RandomMove()
			end
			return
		end
	else
		mX = LuaGetX(mId)
		mY = LuaGetY(mId)
		--LuaPrint("CALL tick: "..mX..","..mY.."\n")
		minDistance = mAgroDistance
		targetActorX = -1
		targetActorY = -1
		for i = 1, #actorIdArray do
			local actorId = actorIdArray[i]
			--LuaPrint("tick loop["..i)
			--LuaPrint("]: "..actorId.."\n")
			actorX = LuaGetX(actorId)
			actorY = LuaGetY(actorId)
			curDistance = Distance(mX, mY, actorX, actorY)
			if(minDistance > curDistance) then
				minDistance = curDistance
				targetActorX = actorX
				targetActorY = actorY
			end
		end
		if(targetActorX == -1 or targetActorY == -1) then
			--어그로 안끌릴때 행동
			if(ESoloMove_Roaming == mSoloMove) then
				RandomMove()
			end
			return
		end
	end
	
	--LuaPrint("center: "..targetActorX..","..mX.."/"..targetActorY..","..mY.."\n")
	if(targetActorX - mX > 0) then
		mX = mX+1
	elseif(targetActorX - mX < 0) then
		mX = mX-1
	elseif(targetActorY - mY > 0) then
		mY = mY+1
	elseif(targetActorY - mY < 0) then
		mY = mY-1
	end
	--LuaPrint("after tick: "..mX..","..mY.."\n")
	LuaSetPos(mId, mX, mY)
end

function OnNearActorWithPlayerMove(playerId)
end

function OnNearActorWithSelfMove(playerId)
	playerX = LuaGetX(playerId)
	playerY = LuaGetY(playerId)
	mX = LuaGetX(mId)
	mY = LuaGetY(mId)

	if(playerX == mX) then
		if(playerY == mY) then
			LuaTakeDamage(playerId, mId)
		end
	end
end

function TakeDamage(playerId, damage)
	result = true
	mTargetActorId = playerId
	mHp = mHp - damage
	if(mHp <= 0) then
		mHp = 0
		playerhp = LauGetHp(playerId)
		playerlevel = LuaGetLevel(playerId)
		playerexp = LuaGetExp(playerId)
		playerexp = playerexp + mExp
		LuaSendStatChange(playerId, mId, playerhp, playerlevel, playerexp); -- 경험치 습득
		result = false
	end
	LuaPrint("take_damage("..playerId..":"..damage..","..mHp..")\n")
	LuaSendStatChange(mId, mId, mHp, mLevel, mExp);
	return result
end

function GetHp()
	return mHp
end
function GetLevel()
	return mLevel
end
function GetExp()
	return mExp
end
function GetDamage()
	return mDamage
end