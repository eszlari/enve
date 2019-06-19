#include "document.h"
#include "FileCacheHandlers/filecachehandler.h"
#include "canvas.h"
Document* Document::sInstance = nullptr;

bool Document::FileCompare::operator()(const FileHandler &f1,
                                       const FileHandler &f2) {
    return f1->getFilePath() < f2->getFilePath();
}

Canvas *Document::createNewScene() {
    const auto newScene = SPtrCreate(Canvas)(*this);
    fScenes.append(newScene);
    SWT_addChild(newScene.get());
    emit sceneCreated(newScene.get());
    return newScene.get();
}

bool Document::removeScene(const qsptr<Canvas>& scene) {
    const int id = fScenes.indexOf(scene);
    return removeScene(id);
}

bool Document::removeScene(const int id) {
    if(id < 0 || id >= fScenes.count()) return false;
    const auto scene = fScenes.takeAt(id);
    SWT_removeChild(scene.data());
    emit sceneRemoved(scene.data());
    emit sceneRemoved(id);
    return true;
}

void Document::setActiveScene(Canvas * const scene) {
    if(scene == fActiveScene) return;
    if(fActiveScene) {
        disconnect(fActiveScene, nullptr, this, nullptr);
    }
    fActiveScene = scene;
    if(fActiveScene) {
        connect(fActiveScene, &Canvas::boxSelectionChanged,
                this, &Document::activeSceneBoxSelectionChanged);
        connect(fActiveScene, &Canvas::selectedPaintSettingsChanged,
                this, &Document::selectedPaintSettingsChanged);
    }
    SWT_scheduleContentUpdate(scene ? scene->getCurrentGroup() : nullptr,
                              SWT_TARGET_CURRENT_GROUP);
    SWT_scheduleContentUpdate(scene, SWT_TARGET_CURRENT_CANVAS);
    emit activeSceneSet(scene);
    emit activeSceneBoxSelectionChanged();
}

Gradient *Document::createNewGradient() {
    const auto grad = SPtrCreate(Gradient)();
    fGradients.append(grad);
    emit gradientCreated(grad.get());
    return grad.get();
}

Gradient *Document::duplicateGradient(const int id) {
    if(id < 0 || id >= fGradients.count()) return nullptr;
    const auto from = fGradients.at(id).get();
    const auto newGrad = createNewGradient();
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    from->write(-1, &buffer);
    if(buffer.reset()) newGrad->read(&buffer);
    buffer.close();
    return newGrad;
}

bool Document::removeGradient(const qsptr<Gradient> &gradient) {
    const int id = fGradients.indexOf(gradient);
    return removeGradient(id);
}

bool Document::removeGradient(const int id) {
    if(id < 0 || id >= fGradients.count()) return false;
    const auto grad = fGradients.takeAt(id);
    emit gradientRemoved(grad.data());
    emit gradientRemoved(id);
    return true;
}

void Document::clear() {
    for(const auto& scene : fScenes)
        SWT_removeChild(scene.data());
    fScenes.clear();
}

void Document::SWT_setupAbstraction(SWT_Abstraction * const abstraction,
                                    const UpdateFuncs &updateFuncs,
                                    const int visiblePartWidgetId) {
    for(const auto& scene : fScenes) {
        auto abs = scene->SWT_abstractionForWidget(updateFuncs,
                                                   visiblePartWidgetId);
        abstraction->addChildAbstraction(abs);
    }
}