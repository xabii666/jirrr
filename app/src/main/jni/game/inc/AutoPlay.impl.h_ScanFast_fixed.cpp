void AutoPlay::ScanFast(double angleStep) {
    if (g_CurrentCandidate.idx != -1) return;
    
    auto& cueBall = gPrediction->guiData.balls[0];
    double distSq = (cueBall.initialPosition - fs.scanCuePos).square();
    
    if (!fs.isInitiated || distSq > 0.0025) {
        fs.raw.clear();
        fs.evals.clear();
        fs.evalIndex = 0;
        fs.scanCuePos = cueBall.initialPosition;
        fs.isInitiated = true;
        fs.prepPhase = 0; 
    }

    if (fs.prepPhase < 6) {
        auto myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        bool isNineBall = (myclass == Ball::Classification::NINE_BALL_RULE);
        Table table = sharedGameManager.mTable;
        if (!table) return;
        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;
        auto& pocketArr = tableProperties.mPockets();
        Vec2d* pockets = &pocketArr[0];

        int ballsPerPhase = 3; 
        int startBall = fs.prepPhase * ballsPerPhase + 1;
        int endBall = std::min(startBall + ballsPerPhase, (int)gPrediction->guiData.ballsCount);
        
        for (int i = startBall; i < endBall; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;
            if (!isNineBall) {
                bool isACand = (myclass == Ball::Classification::ANY) ? (ball.classification != Ball::Classification::EIGHT_BALL) : (ball.classification == myclass);
                if (!isACand) continue;
            } else if (i > 1 && !fs.raw.empty()) break; 

            for (int pIdx = 0; pIdx < 6; pIdx++) {
                if (nominatedPocket < 6 && pIdx != nominatedPocket) continue;
                
                // 1. Direct
                {
                    Point2D toPocket = pockets[pIdx] - ball.initialPosition;
                    double distP = sqrt(toPocket.square());
                    if (distP > 0.1) {
                        Point2D ghost = ball.initialPosition - (toPocket * (1.0 / distP)) * (2.0 * BALL_RADIUS);
                        Point2D shot = ghost - cueBall.initialPosition;
                        double distC = sqrt(shot.square());
                        double angle = atan2(shot.y, shot.x);
                        double basePower = CalculateRequiredPower(distC + distP);
                        double powerSteps[] = { basePower * 0.9, basePower, basePower * 1.15 };
                        for (double p : powerSteps) {
                            double finalP = p;
                            if (cleanTableMode == CLEAN_ALL_BALLS) finalP = powerMax;
                            else { if (finalP > powerMax) finalP = powerMax; if (finalP < powerMin) finalP = powerMin; }
                            fs.raw.push_back({i, angle, 0, pIdx, finalP, distC + distP});
                        }
                    }
                }

                // 2. Bank
                if (bCushionShot) {
                    for (int side = 0; side < 4; side++) {
                        Point2D mp;
                        switch(side) {
                            case 0: mp = {pockets[pIdx].x, -pockets[pIdx].y}; break;
                            case 1: mp = {pockets[pIdx].x, 192.0 - pockets[pIdx].y}; break;
                            case 2: mp = {-pockets[pIdx].x, pockets[pIdx].y}; break;
                            case 3: mp = {384.0 - pockets[pIdx].x, pockets[pIdx].y}; break;
                        }
                        Point2D toMir = mp - ball.initialPosition;
                        double distP = sqrt(toMir.square());
                        if (distP > 0.1) {
                            Point2D ghost = ball.initialPosition - (toMir * (1.0 / distP)) * (2.0 * BALL_RADIUS);
                            Point2D shot = ghost - cueBall.initialPosition;
                            double distC = sqrt(shot.square());
                            double angle = atan2(shot.y, shot.x);
                            double basePower = sqrt(220.0 * (distC + distP)) * 1.5; 
                            double powerSteps[] = { basePower * 0.9, basePower, basePower * 1.15 };
                            for (double p : powerSteps) {
                                double finalP = p;
                                if (finalP > powerMax) finalP = powerMax;
                                if (finalP < powerMin + 150) finalP = powerMin + 150;
                                fs.raw.push_back({i, angle, 0, pIdx, finalP, distC + distP + 500});
                            }
                        }
                    }
                }
            }
        }
        fs.prepPhase++;
        if (fs.prepPhase == 6) {
            std::sort(fs.raw.begin(), fs.raw.end());
            if (fs.raw.size() > 500) fs.raw.resize(500); 
        }
        return; 
    }

    if (automationSpeed == SPEED_HUMAN && humanShotLocked) return;

    auto myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    bool isNineBall = (myclass == Ball::Classification::NINE_BALL_RULE);
    int stepsInThisFrame = 0;
    const int maxStepsPerFrame = 30; 

    while (stepsInThisFrame < maxStepsPerFrame && fs.evalIndex < fs.raw.size()) {
        const auto& r = fs.raw[fs.evalIndex++];
        sweepAngle = r.angle;
        stepsInThisFrame++;

        gPrediction->forceFullSimulation = bShowAutoPlayLines; 
        gPrediction->determineShotResult(true, r.angle, r.power, sharedGameManager.getShotSpin());
        gPrediction->forceFullSimulation = false;
        
        bool validShot = true;
        if (!gPrediction->guiData.balls[0].onTable) validShot = false;

        if (validShot) {
            bool p8 = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable && i == 8) p8 = true;
            }
            if (p8 && myclass != Ball::Classification::EIGHT_BALL) validShot = false;
        }

        if (validShot) {
            auto firstHit = gPrediction->guiData.collision.firstHitBall;
            if (!firstHit) validShot = false;
            else {
                bool hitTarget = (firstHit->index == r.idx);
                bool hitLegal = (myclass == Ball::Classification::ANY) ? (firstHit->classification != Ball::Classification::EIGHT_BALL) : (firstHit->classification == myclass);
                if (!hitTarget && !hitLegal) validShot = false;
            }
        }

        if (validShot) {
            int tot = 0, own = 0; bool hasLegal = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                    tot++;
                    bool m = (myclass == Ball::Classification::ANY) ? (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : (gPrediction->guiData.balls[i].classification == myclass);
                    if (m) { hasLegal = true; own++; }
                }
            }
            
            if (hasLegal) {
                int totalScore = (own * 1000);
                double distFactor = (500.0 - r.dist) / 10.0; 
                if (distFactor > 0) totalScore += (int)distFactor;

                int ownBallsMoved = 0;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (b.classification == myclass && b.originalOnTable && b.onTable) {
                        double moveDistSq = (b.initialPosition - b.predictedPosition).square();
                        if (moveDistSq > 1.0) ownBallsMoved++;
                    }
                }
                totalScore += (ownBallsMoved * 50);

                Candidate cf = r; cf.score = (double)totalScore;
                fs.evals.push_back({cf, totalScore, own, false});
            }
        }
    }

    if (fs.evalIndex >= fs.raw.size()) {
        FastScanState::Eval* best = nullptr;
        for (auto& ev : fs.evals) {
            if (!best || ev.c.score > best->c.score) best = &ev;
        }

        if (best) {
            fs.isInitiated = false;
            g_CurrentCandidate = best->c;
            Shoot(best->c.angle, best->c.power);
        } else {
            fs.isInitiated = false;
            lastFailedCuePos = cueBall.initialPosition;
            scan = SLOW;
        }
    }
}
