mId =  -1
mInitX = -1
mInitY = -1
mX = -1
mY = -1
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
mCanFindWay = false
mTargetActorId = -1
mIsFindTarget = -1
mPathTargetY = -1
mIsFindWayStart = false

function SetId(x)
	mId = x
	mInitX = LuaGetX(mId)
	mX = mInitX
	mInitY = LuaGetY(mId)
	mY = mInitY
	InitStat()
end

function Reset(threadIdx)
	mX = mInitX
	mY = mInitY
	mIsFindWayStart = false
	LuaSetPos(mId, mX, mY, threadIdx)
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

function TakeDamage(playerId, damage, threadIdx)
	result = true
	targetActorX = LuaGetX(playerId)
	targetActorY = LuaGetY(playerId)
	mTargetActorId = playerId
	mHp = mHp - damage
	if(mHp <= 0) then
		mHp = 0
		playerhp = LauGetHp(playerId)
		playerlevel = LuaGetLevel(playerId)
		playerexp = LuaGetExp(playerId)
		playerexp = playerexp + mExp
		LuaSendStatChange(playerId, mId, playerhp, playerlevel, playerexp, threadIdx); -- ����ġ ����
		result = false
	end
	LuaSendStatChange(mId, mId, mHp, mLevel, mExp, threadIdx);
	return result
end

function OnNearActorWithPlayerMove(playerId, threadIdx)
end

function Tick(threadIdx)
	if(mTargetActorId ~= -1) then
		otherX = LuaGetX(mTargetActorId)
		otherY = LuaGetY(mTargetActorId)

		if(otherX == mX and otherY == mY) then
			LuaTakeDamage(mTargetActorId, mId, threadIdx)
		end
	end

	if(EFindPlayerAct_Peace == mFindPlayerAct) then
		if(ESoloMove_Roaming == mSoloMove) then -- �ι�
			LuaRandomMove(mId, threadIdx)
		end
	else -- ��ó���� ��׷� ������
		if(-1 == mTargetActorId) then
			nearestPlayer = LuaGetNearPlayer(mId)
			if(-1 ~= nearestPlayer) then
				targetActorX = LuaGetX(nearestPlayer)
				targetActorY = LuaGetY(nearestPlayer)
				mTargetActorId = nearestPlayer
			elseif(targetActorX == -1 or targetActorY == -1) then
				--��׷� �Ȳ����� �ൿ
				if(ESoloMove_Roaming == mSoloMove) then
					LuaRandomMove(mId, threadIdx)
				end
				return
			end
		end
	end
	
	-- �÷��̾�� �ٰ����� �ൿ
	if(mCanFindWay == true) then -- ��ã�� ����
		if(mTargetActorId ~= -1 and mIsFindWayStart == false) then
			LuaSetPathStartAndTarget(mId, targetActorX, targetActorY)
			mIsFindWayStart = true
			LuaPrint("LuaSetPathStartAndTarget("..mId..", "..targetActorX..", "..targetActorY..")\n");
		end
		if(mX == targetActorX and mY == targetActorY) then -- ��ǥ�� �����ϸ� ��ǥ�� �̵��������� �𸣴� ��ġ�� �̵��Ѵ�
			targetActorX = LuaGetX(mTargetActorId)
			targetActorY = LuaGetY(mTargetActorId)
			LuaSetPathStartAndTarget(mId, targetActorX, targetActorY)
			LuaPrint("mX == targetActorX and mY == targetActorY LuaSetPathStartAndTarget("..mId..", "..targetActorX..", "..targetActorY..")\n");
		end
	elseif(mTargetActorId ~= -1) then
		-- �� �� ã�� ����
		targetActorX = LuaGetX(mTargetActorId)
		targetActorY = LuaGetY(mTargetActorId)
		LuaMoveToConsiderWall(mId, targetActorX, targetActorY, threadIdx)
	end
end
