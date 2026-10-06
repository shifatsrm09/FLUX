#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

namespace Flux {

class VLCPlayer;
class TestMediaModel;

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(Flux::VLCPlayer* player READ player CONSTANT)
    Q_PROPERTY(Flux::TestMediaModel* testMedia READ testMedia CONSTANT)

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    bool initialize(QQmlApplicationEngine &engine);

    VLCPlayer* player() const { return m_player.get(); }
    TestMediaModel* testMedia() const { return m_testMedia.get(); }

private:
    std::unique_ptr<VLCPlayer> m_player;
    std::unique_ptr<TestMediaModel> m_testMedia;
};

} // namespace Flux
