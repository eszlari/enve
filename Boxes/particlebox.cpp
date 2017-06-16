#include "particlebox.h"
#include "mainwindow.h"
#include "durationrectangle.h"
#include "Animators/animatorupdater.h"
#include "canvas.h"
#include "pointhelpers.h"

double fRand(double fMin, double fMax) {
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

ParticleBox::ParticleBox(BoxesGroup *parent) :
    BoundingBox(parent, TYPE_PARTICLES) {
    setName("Particle Box");
    mTopLeftPoint = new MovablePoint(this, TYPE_PATH_POINT);
    mBottomRightPoint = new MovablePoint(this, TYPE_PATH_POINT);

    ca_addChildAnimator(mTopLeftPoint);
    ca_addChildAnimator(mBottomRightPoint);

    mTopLeftPoint->prp_setUpdater(
                new DisplayedFillStrokeSettingsUpdater(this));
    mTopLeftPoint->prp_setName("top left");
    mBottomRightPoint->prp_setUpdater(
                new DisplayedFillStrokeSettingsUpdater(this));
    mBottomRightPoint->prp_setName("bottom right");

    VaryingLenAnimationRect *durRect = new VaryingLenAnimationRect(this);
    setDurationRectangle(durRect);
    durRect->setMaxFrame(200);
    durRect->setMinFrame(-10);

    //addEmitter(new ParticleEmitter(this));
}

void ParticleBox::getAccelerationAt(const QPointF &pos,
                                    const int &frame,
                                    QPointF *acc) {
    Q_UNUSED(pos);
    Q_UNUSED(frame);
    *acc = QPointF(0., 9.8)/24.;
}

void ParticleBox::updateAfterFrameChanged(const int &currentFrame) {
    BoundingBox::updateAfterFrameChanged(currentFrame);
    scheduleSoftUpdate();
}

void ParticleBox::updateRelBoundingRect() {
    mRelBoundingRect = QRectF(mTopLeftPoint->getRelativePos(),
                              mBottomRightPoint->getRelativePos());
    mRelBoundingRectSk = QRectFToSkRect(mRelBoundingRect);
//    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
//        mRelBoundingRect = mRelBoundingRect.united(
//                    emitter->getParticlesBoundingRect());
//    }

    BoundingBox::updateRelBoundingRect();
}

void ParticleBox::preUpdatePixmapsUpdates() {
    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        emitter->generateParticlesIfNeeded();
        emitter->updateParticlesForFrameIfNeeded(mUpdateRelFrame);
    }

    BoundingBox::preUpdatePixmapsUpdates();
}

bool ParticleBox::relPointInsidePath(const QPointF &relPos) {
    if(mSkRelBoundingRectPath.contains(relPos.x(), relPos.y()) ) {
        /*if(mEmitters.isEmpty()) */return true;
//        Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
//            if(emitter->relPointInsidePath(relPos)) {
//                return true;
//            }
//        }
//        return false;
    } else {
        return false;
    }
}

void ParticleBox::addEmitter(ParticleEmitter *emitter) {
    mEmitters << emitter;
    ca_addChildAnimator(emitter);
}

BoundingBox *ParticleBox::createNewDuplicate(BoxesGroup *parent) {
    return new ParticleBox(parent);
}

void ParticleBox::makeDuplicate(Property *targetBox) {
    BoundingBox::makeDuplicate(targetBox);
    ParticleBox *pbTarget = (ParticleBox*)targetBox;
    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        pbTarget->addEmitter((ParticleEmitter*)
                             emitter->makeDuplicate());
    }
}

void ParticleBox::addEmitterAtAbsPos(const QPointF &absPos) {
    ParticleEmitter *emitter = new ParticleEmitter(this);
    emitter->getPosPoint()->setAbsolutePos(absPos, false);
    addEmitter(emitter);
}

void ParticleBox::prp_setAbsFrame(const int &frame) {
    BoundingBox::prp_setAbsFrame(frame);
    mFrameChangedUpdateScheduled = true;
}

void ParticleBox::setUpdateVars() {
    BoundingBox::setUpdateVars();
    if(mFrameChangedUpdateScheduled) {
        mFrameChangedUpdateScheduled = false;
        Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
            emitter->scheduleUpdateParticlesForFrame();
        }
    }
}

bool ParticleBox::SWT_isParticleBox() { return true; }

void ParticleBox::updateAfterDurationRectangleRangeChanged() {
    int minFrame = mDurationRectangle->getMinFrameAsRelFrame();
    int maxFrame = mDurationRectangle->getMaxFrameAsRelFrame();
    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        emitter->setFrameRange(minFrame, maxFrame);
    }
}

void ParticleBox::drawSk(SkCanvas *canvas) {
    canvas->save();

    canvas->clipRect(QRectFToSkRect(mRelBoundingRect));
    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        emitter->drawParticlesSk(canvas);
    }

    //p->setCompositionMode(QPainter::CompositionMode_DestinationIn);
    //p->fillRect(mRelBoundingRect, Qt::white);
    canvas->restore();
}

void ParticleBox::applyPaintSetting(const PaintSetting &setting) {
    if(setting.targetsFill()) {
        Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
            setting.applyColorSetting(emitter->getColorAnimator());
        }
    }
}

void ParticleBox::startAllPointsTransform() {
    mBottomRightPoint->startTransform();
    mTopLeftPoint->startTransform();
    startTransform();
}

void ParticleBox::drawSelectedSk(SkCanvas *canvas,
                                 const CanvasMode &currentCanvasMode,
                                 const SkScalar &invScale) {
    if(isVisibleAndInVisibleDurationRect()) {
        canvas->save();
        drawBoundingRectSk(canvas, invScale);
        if(currentCanvasMode == CanvasMode::MOVE_POINT) {
            mTopLeftPoint->drawSk(canvas, invScale);
            mBottomRightPoint->drawSk(canvas, invScale);
            Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
                MovablePoint *pt = emitter->getPosPoint();
                pt->drawSk(canvas, invScale);
            }
        } else if(currentCanvasMode == MOVE_PATH) {
            mTransformAnimator->getPivotMovablePoint()->
                    drawSk(canvas, invScale);
        }
        canvas->restore();
    }
}

MovablePoint *ParticleBox::getPointAtAbsPos(const QPointF &absPtPos,
                                      const CanvasMode &currentCanvasMode,
                                      const qreal &canvasScaleInv) {
    if(currentCanvasMode == MOVE_POINT) {
        if(mTopLeftPoint->isPointAtAbsPos(absPtPos, canvasScaleInv)) {
            return mTopLeftPoint;
        }
        if(mBottomRightPoint->isPointAtAbsPos(absPtPos, canvasScaleInv) ) {
            return mBottomRightPoint;
        }
    } else if(currentCanvasMode == MOVE_PATH) {
        MovablePoint *pivotMovable = mTransformAnimator->getPivotMovablePoint();
        if(pivotMovable->isPointAtAbsPos(absPtPos, canvasScaleInv)) {
            return pivotMovable;
        }
    }

    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        MovablePoint *pt = emitter->getPosPoint();
        if(pt->isPointAtAbsPos(absPtPos, canvasScaleInv)) {
            return pt;
        }
    }
    return NULL;
}

void ParticleBox::selectAndAddContainedPointsToList(const QRectF &absRect,
                                                  QList<MovablePoint *> *list) {
    if(!mTopLeftPoint->isSelected()) {
        if(mTopLeftPoint->isContainedInRect(absRect)) {
            mTopLeftPoint->select();
            list->append(mTopLeftPoint);
        }
    }
    if(!mBottomRightPoint->isSelected()) {
        if(mBottomRightPoint->isContainedInRect(absRect)) {
            mBottomRightPoint->select();
            list->append(mBottomRightPoint);
        }
    }
    Q_FOREACH(ParticleEmitter *emitter, mEmitters) {
        MovablePoint *pt = emitter->getPosPoint();
        if(pt->isContainedInRect(absRect)) {
            pt->select();
            list->append(pt);
        }
    }
}

MovablePoint *ParticleBox::getBottomRightPoint() {
    return mBottomRightPoint;
}

Particle::Particle(ParticleBox *parentBox) {
    mParentBox = parentBox;
}

void Particle::initializeParticle(const int &firstFrame,
                                  const int &nFrames,
                                  const SkPoint &iniPos,
                                  const SkPoint &iniVel,
                                  const SkScalar &partSize) {
    mSize = partSize;
    mPrevVelocityVar = SkPoint::Make(0., 0.);
    mNextVelocityVar = SkPoint::Make(0., 0.);
    mPrevVelocityDuration = 10000000.;
    mLastScale = 1.;
    mLastOpacity = 1.;

    mFirstFrame = firstFrame;
    mLastPos = iniPos;
    mLastVel = iniVel;
    if(mParticleStates != NULL) {
        if(nFrames == mNumberFrames) return;
        delete[] mParticleStates;
    }
    mNumberFrames = nFrames;
    mParticleStates = new ParticleState[nFrames];
}

void Particle::generatePathNextFrame(const int &frame,
                                     const SkScalar &velocityVar,
                                     const SkScalar &velocityVarPeriod,
                                     const SkPoint &acc,
                                     const SkScalar &finalScale,
                                     const SkScalar &finalOpacity,
                                     const SkScalar &decayFrames,
                                     const SkScalar &length) {
    if(mPrevVelocityDuration > velocityVarPeriod) {
        mPrevVelocityVar = mNextVelocityVar;
        mNextVelocityVar = SkPoint::Make(fRand(-velocityVar, velocityVar),
                                         fRand(-velocityVar, velocityVar));
        mPrevVelocityDuration = 0.;
    }

    int arrayId = frame - mFirstFrame;

    if(arrayId == 0) {
        SkScalar iniTime = fRand(0., 1.);
        mLastPos += mLastVel*iniTime;
        mLastVel += acc*iniTime;
    }

    int remaining = mNumberFrames - arrayId;
    if(remaining <= decayFrames) {
        mLastScale += (finalScale - 1.)/decayFrames;
        mLastOpacity += (finalOpacity - 1.)/decayFrames;
    }

    SkPath linePath;
    SkScalar currLen = 0.;
    int currId = arrayId - 1;
    SkPoint lastPos = mLastPos;
    linePath.moveTo(lastPos);
    while(currId > -1) {
        SkPoint currPos = mParticleStates[currId].pos;
        SkScalar lenInc = pointToLen(lastPos - currPos);
        SkScalar newLen = currLen + lenInc;
        if(newLen > length) {
            linePath.lineTo(lastPos + (currPos - lastPos)*
                            (length - currLen)*(1./lenInc));
            break;
        } else {
            linePath.lineTo(currPos);
        }
        currLen = newLen;
        lastPos = currPos;
        currId--;
    }

    mParticleStates[arrayId] =
            ParticleState(mLastPos,
                          mLastScale,
                          mSize,
                          qMax(0, qMin(255, qRound(mLastOpacity*255))),
                          linePath);

    SkScalar perPrevVelVar = (velocityVarPeriod - mPrevVelocityDuration)/
                            velocityVarPeriod;
    mLastPos += mLastVel + mPrevVelocityVar*perPrevVelVar +
                    mNextVelocityVar*(1.f - perPrevVelVar);
    mLastVel += acc;

    mPrevVelocityDuration += 1.;
}

bool Particle::isVisibleAtFrame(const int &frame) {
    int arrayId = frame - mFirstFrame;
    if(arrayId < 0 || arrayId >= mNumberFrames) return false;
    return true;
}

ParticleState Particle::getParticleStateAtFrame(const int &frame) {
    int arrayId = frame - mFirstFrame;
    return mParticleStates[arrayId];
}

ParticleEmitter::ParticleEmitter(ParticleBox *parentBox) :
    ComplexAnimator() {
    setParentBox(parentBox);

    prp_setName("particle emitter");

    mPos = (new MovablePoint(mParentBox, TYPE_PATH_POINT))->ref<MovablePoint>();
    //mPos->setName("pos");
    //mPos.setCurrentValue(QPointF(0., 0.));

    mColorAnimator->prp_setName("color");
    mColorAnimator->qra_setCurrentValue(Color(0, 0, 0));
    ca_addChildAnimator(mColorAnimator.data());

    mWidth->prp_setName("width");
    mWidth->qra_setValueRange(0., 6000.);
    mWidth->qra_setCurrentValue(0.);

    mSrcVelInfl->prp_setName("src vel infl");
    mSrcVelInfl->qra_setValueRange(-1., 1.);
    mSrcVelInfl->qra_setCurrentValue(0.);

    mIniVelocity->prp_setName("ini vel");
    mIniVelocity->qra_setValueRange(-1000., 1000.);
    mIniVelocity->qra_setCurrentValue(10.);

    mIniVelocityVar->prp_setName("ini vel var");
    mIniVelocityVar->qra_setValueRange(0., 1000.);
    mIniVelocityVar->qra_setCurrentValue(5.);

    mIniVelocityAngle->prp_setName("ini vel angle");
    mIniVelocityAngle->qra_setValueRange(-3600., 3600.);
    mIniVelocityAngle->qra_setCurrentValue(-90.);

    mIniVelocityAngleVar->prp_setName("ini vel angle var");
    mIniVelocityAngleVar->qra_setValueRange(0., 3600.);
    mIniVelocityAngleVar->qra_setCurrentValue(15.);

    mAcceleration->prp_setName("acceleration");
    mAcceleration->setValuesRange(-100., 100.);
    mAcceleration->setCurrentPointValue(QPointF(0., 9.8));

    mParticlesPerSecond->prp_setName("particles per second");
    mParticlesPerSecond->qra_setValueRange(0., 10000.);
    mParticlesPerSecond->qra_setCurrentValue(120);

    mParticlesFrameLifetime->prp_setName("particles lifetime");
    mParticlesFrameLifetime->qra_setValueRange(1., 1000.);
    mParticlesFrameLifetime->qra_setCurrentValue(50.);

    mVelocityRandomVar->prp_setName("velocity random var");
    mVelocityRandomVar->qra_setValueRange(0., 1000.);
    mVelocityRandomVar->qra_setCurrentValue(5.);

    mVelocityRandomVarPeriod->prp_setName("velocity random var period");
    mVelocityRandomVarPeriod->qra_setValueRange(1., 100.);
    mVelocityRandomVarPeriod->qra_setCurrentValue(10.);

    mParticleSize->prp_setName("particle size");
    mParticleSize->qra_setValueRange(0., 100.);
    mParticleSize->qra_setCurrentValue(5.);

    mParticleSizeVar->prp_setName("particle size var");
    mParticleSizeVar->qra_setValueRange(0., 100.);
    mParticleSizeVar->qra_setCurrentValue(1.);

    mParticleLength->prp_setName("length");
    mParticleLength->qra_setValueRange(0., 2000.);
    mParticleLength->qra_setCurrentValue(0.);

    mParticlesDecayFrames->prp_setName("decay frames");
    mParticlesDecayFrames->qra_setValueRange(0., 1000.);
    mParticlesDecayFrames->qra_setCurrentValue(10.);

    mParticlesSizeDecay->prp_setName("final scale");
    mParticlesSizeDecay->qra_setValueRange(0., 10.);
    mParticlesSizeDecay->qra_setCurrentValue(0.);

    mParticlesOpacityDecay->prp_setName("final opacity");
    mParticlesOpacityDecay->qra_setValueRange(0., 1.);
    mParticlesOpacityDecay->qra_setCurrentValue(0.);

    mPos->prp_setName("pos");
    ca_addChildAnimator(mPos.data());
    ca_addChildAnimator(mWidth.data());

    ca_addChildAnimator(mSrcVelInfl.data());

    ca_addChildAnimator(mIniVelocity.data());
    ca_addChildAnimator(mIniVelocityVar.data());

    ca_addChildAnimator(mIniVelocityAngle.data());
    ca_addChildAnimator(mIniVelocityAngleVar.data());

    ca_addChildAnimator(mAcceleration.data());

    ca_addChildAnimator(mParticlesPerSecond.data());
    ca_addChildAnimator(mParticlesFrameLifetime.data());

    ca_addChildAnimator(mVelocityRandomVar.data());
    ca_addChildAnimator(mVelocityRandomVarPeriod.data());

    ca_addChildAnimator(mParticleSize.data());
    ca_addChildAnimator(mParticleSizeVar.data());

    ca_addChildAnimator(mParticleLength.data());

    ca_addChildAnimator(mParticlesDecayFrames.data());
    ca_addChildAnimator(mParticlesSizeDecay.data());
    ca_addChildAnimator(mParticlesOpacityDecay.data());

    prp_setUpdater(new ParticlesUpdater(this));
    prp_blockUpdater();
}

void ParticleEmitter::setParentBox(ParticleBox *parentBox) {
    mParentBox = parentBox;

    scheduleGenerateParticles();
    if(parentBox == NULL) {
        mColorAnimator->prp_setUpdater(NULL);
    } else {
        mColorAnimator->prp_setUpdater(
                    new DisplayedFillStrokeSettingsUpdater(parentBox));
    }
}

void ParticleEmitter::scheduleGenerateParticles() {
    mGenerateParticlesScheduled = true;
    mParentBox->clearAllCache();
    mParentBox->scheduleSoftUpdate();
}

void ParticleEmitter::scheduleUpdateParticlesForFrame() {
    mUpdateParticlesForFrameScheduled = true;
    mParentBox->scheduleSoftUpdate();
}

void ParticleEmitter::updateParticlesForFrameIfNeeded(const int &frame) {
    if(mUpdateParticlesForFrameScheduled) {
        mUpdateParticlesForFrameScheduled = false;
        updateParticlesForFrame(frame);
    }
}

bool ParticleEmitter::relPointInsidePath(const SkPoint &relPos) {
    Q_FOREACH(const ParticleState &state, mParticleStates) {
        if(pointToLen(state.pos - relPos) < 5.) {
            return true;
        }
    }

    return false;
}

Property *ParticleEmitter::makeDuplicate() {
    ParticleEmitter *emitterDupli = new ParticleEmitter(mParentBox);
    makeDuplicate(emitterDupli);
    return emitterDupli;
}

void ParticleEmitter::duplicateAnimatorsFrom(QPointFAnimator *pos,
                                             QrealAnimator *width,
                                             QrealAnimator *srcVelInfl,
                                             QrealAnimator *iniVelocity,
                                             QrealAnimator *iniVelocityVar,
                                             QrealAnimator *iniVelocityAngle,
                                             QrealAnimator *iniVelocityAngleVar,
                                             QPointFAnimator *acceleration,
                                             QrealAnimator *particlesPerSecond,
                                             QrealAnimator *particlesFrameLifetime,
                                             QrealAnimator *velocityRandomVar,
                                             QrealAnimator *velocityRandomVarPeriod,
                                             QrealAnimator *particleSize,
                                             QrealAnimator *particleSizeVar,
                                             QrealAnimator *particleLength,
                                             QrealAnimator *particlesDecayFrames,
                                             QrealAnimator *particlesSizeDecay,
                                             QrealAnimator *particlesOpacityDecay) {
    pos->makeDuplicate(mPos.data());
    width->makeDuplicate(mWidth.data());

    srcVelInfl->makeDuplicate(mSrcVelInfl.data());

    iniVelocity->makeDuplicate(mIniVelocity.data());
    iniVelocityVar->makeDuplicate(mIniVelocityVar.data());
    iniVelocityAngle->makeDuplicate(mIniVelocityAngle.data());
    iniVelocityAngleVar->makeDuplicate(mIniVelocityAngleVar.data());
    acceleration->makeDuplicate(mAcceleration.data());

    particlesPerSecond->makeDuplicate(mParticlesPerSecond.data());
    particlesFrameLifetime->makeDuplicate(mParticlesFrameLifetime.data());

    velocityRandomVar->makeDuplicate(mVelocityRandomVar.data());
    velocityRandomVarPeriod->makeDuplicate(mVelocityRandomVarPeriod.data());

    particleSize->makeDuplicate(mParticleSize.data());
    particleSizeVar->makeDuplicate(mParticleSizeVar.data());

    particleLength->makeDuplicate(mParticleLength.data());

    particlesDecayFrames->makeDuplicate(mParticlesDecayFrames.data());
    particlesSizeDecay->makeDuplicate(mParticlesSizeDecay.data());
    particlesOpacityDecay->makeDuplicate(mParticlesOpacityDecay.data());
}

void ParticleEmitter::makeDuplicate(Property *target) {
    ParticleEmitter *peTarget = ((ParticleEmitter*)target);
    peTarget->duplicateAnimatorsFrom(
                mPos.data(),
                mWidth.data(),
                mSrcVelInfl.data(),
                mIniVelocity.data(),
                mIniVelocityVar.data(),
                mIniVelocityAngle.data(),
                mIniVelocityAngleVar.data(),
                mAcceleration.data(),
                mParticlesPerSecond.data(),
                mParticlesFrameLifetime.data(),
                mVelocityRandomVar.data(),
                mVelocityRandomVarPeriod.data(),
                mParticleSize.data(),
                mParticleSizeVar.data(),
                mParticleLength.data(),
                mParticlesDecayFrames.data(),
                mParticlesSizeDecay.data(),
                mParticlesOpacityDecay.data());
    peTarget->setFrameRange(mMinFrame, mMaxFrame);
}

void ParticleEmitter::setMinFrame(const int &minFrame) {
    mMinFrame = minFrame;
    scheduleGenerateParticles();
}

void ParticleEmitter::setMaxFrame(const int &maxFrame) {
    mMaxFrame = maxFrame;
    scheduleGenerateParticles();
}

void ParticleEmitter::setFrameRange(const int &minFrame, const int &maxFrame) {
    if(minFrame == mMinFrame && mMaxFrame == maxFrame) return;
    if(mMaxFrame > maxFrame) {
        int currId = mParticles.count() - 1;
        while(currId > 0) {
            Particle *part = mParticles.at(currId);
            if(part->isVisibleAtFrame(maxFrame)) break;
            mParticles.removeOne(part);
            currId--;
            delete part;
        }
    } else {
        scheduleGenerateParticles();
    }

    mMinFrame = minFrame;
    mMaxFrame = maxFrame;
}

ColorAnimator *ParticleEmitter::getColorAnimator() {
    return mColorAnimator.data();
}

MovablePoint *ParticleEmitter::getPosPoint() {
    return mPos.data();
}

void ParticleEmitter::generateParticlesIfNeeded() {
    if(mGenerateParticlesScheduled) {
        mGenerateParticlesScheduled = false;
        generateParticles();
    }
}

void ParticleEmitter::generateParticles() {
    srand(0);
    qreal remainingPartFromFrame = 0.;
    QList<Particle*> notFinishedParticles;
    int nReuseParticles = mParticles.count();
    int currentReuseParticle = 0;
    bool reuseParticle = nReuseParticles > 0;

    int totalNeededParticles = 0;
    QPointF lastPos = mPos->getCurrentPointValueAtRelFrame(mMinFrame);
    for(int i = mMinFrame; i < mMaxFrame; i++) {
        qreal srcVelInfl =
                mSrcVelInfl->getCurrentValueAtRelFrame(i);
        qreal iniVelocity =
                mIniVelocity->getCurrentValueAtRelFrame(i);
        qreal iniVelocityVar =
                mIniVelocityVar->getCurrentValueAtRelFrame(i);
        qreal iniVelocityAngle =
                mIniVelocityAngle->getCurrentValueAtRelFrame(i);
        qreal iniVelocityAngleVar =
                mIniVelocityAngleVar->getCurrentValueAtRelFrame(i);
        qreal particlesPerFrame =
                mParticlesPerSecond->getCurrentValueAtRelFrame(i)/24.;
        qreal particlesFrameLifetime =
                mParticlesFrameLifetime->getCurrentValueAtRelFrame(i);
        QPointF pos =
                mPos->getCurrentPointValueAtRelFrame(i);
        qreal width =
                mWidth->getCurrentValueAtRelFrame(i);
        qreal velocityVar =
                mVelocityRandomVar->getCurrentValueAtRelFrame(i);
        qreal velocityVarPeriod =
                mVelocityRandomVarPeriod->getCurrentValueAtRelFrame(i);
        QPointF acceleration =
                mAcceleration->getCurrentPointValueAtRelFrame(i)/24.;
        qreal finalScale =
                mParticlesSizeDecay->getCurrentValueAtRelFrame(i);
        qreal finalOpacity =
                mParticlesOpacityDecay->getCurrentValueAtRelFrame(i);
        qreal decayFrames =
                mParticlesDecayFrames->getCurrentValueAtRelFrame(i);
        qreal particleSize =
                mParticleSize->getCurrentValueAtRelFrame(i);
        qreal particleSizeVar =
                mParticleSizeVar->getCurrentValueAtRelFrame(i);
        qreal length = mParticleLength->getCurrentValueAtRelFrame(i);

        QPointF srcVel = (pos - lastPos)*srcVelInfl;

        int particlesToCreate = remainingPartFromFrame + particlesPerFrame;
        remainingPartFromFrame += particlesPerFrame - particlesToCreate;
        if(remainingPartFromFrame < 0.) remainingPartFromFrame = 0.;

        for(int j = 0; j < particlesToCreate; j++) {
            Particle *newParticle;
            if(reuseParticle) {
                newParticle = mParticles.at(currentReuseParticle);
                currentReuseParticle++;
                reuseParticle = currentReuseParticle < nReuseParticles;
            } else {
                newParticle = new Particle(mParentBox);
                mParticles << newParticle;
            }
            qreal partVelAmp = fRand(iniVelocity - iniVelocityVar,
                                     iniVelocity + iniVelocityVar);


            QMatrix rotVelM;
            qreal velDeg = fRand(iniVelocityAngle - iniVelocityAngleVar,
                                 iniVelocityAngle + iniVelocityAngleVar);
            rotVelM.rotate(velDeg);
            QPointF partVel = rotVelM.map(QPointF(partVelAmp, 0.)) + srcVel;

            qreal partSize = fRand(particleSize - particleSizeVar,
                                   particleSize + particleSizeVar);

            qreal xTrans = fRand(-width, width);

            newParticle->initializeParticle(i, particlesFrameLifetime,
                                            SkPoint::Make(
                                                pos.x() + xTrans,
                                                pos.y()),
                                            QPointFToSkPoint(partVel),
                                            partSize);
            notFinishedParticles << newParticle;
        }
        int nNotFinished = notFinishedParticles.count();
        int currPart = 0;
        while(currPart < nNotFinished) {
            Particle *particle = notFinishedParticles.at(currPart);

            if(particle->isVisibleAtFrame(i)) {
                particle->generatePathNextFrame(i,
                                                velocityVar,
                                                velocityVarPeriod,
                                                QPointFToSkPoint(acceleration),
                                                finalScale,
                                                finalOpacity,
                                                decayFrames,
                                                length);
                currPart++;
            } else {
                notFinishedParticles.removeAt(currPart);
                nNotFinished--;
            }
        }
        totalNeededParticles += particlesToCreate;
        lastPos = pos;
    }
    int nToRemove = mParticles.count() - totalNeededParticles;
    for(int i = 0; i < nToRemove; i++) {
        delete mParticles.takeLast();
    }

    mUpdateParticlesForFrameScheduled = true;
}

void ParticleEmitter::drawParticlesSk(SkCanvas *canvas) {
    canvas->save();
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(mColorAnimator->getCurrentColor().getSkColor());
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStyle(SkPaint::kStroke_Style);
    Q_FOREACH(const ParticleState &state, mParticleStates) {
        state.drawSk(canvas, paint);
    }
    canvas->restore();
}

void ParticleEmitter::updateParticlesForFrame(const int &frame) {
    mParticleStates.clear();
    Q_FOREACH(Particle *particle, mParticles) {
        if(particle->isVisibleAtFrame(frame)) {
            mParticleStates << particle->getParticleStateAtFrame(frame);
        }
    }
}
