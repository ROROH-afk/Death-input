#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <deque>

using namespace geode::prelude;

namespace {
    struct PathPoint { uint64_t frame; CCPoint pos; };
    struct Mark      { uint64_t frame; CCPoint pos; bool down; };

    struct State {
        std::deque<PathPoint> path;
        std::deque<Mark> marks;
        uint64_t frame = 0;
        Ref<CCDrawNode> overlay = nullptr;

        void trim() {
            auto keep = static_cast<uint64_t>(
                Mod::get()->getSettingValue<double>("history-seconds") * 60.0
            );
            while (!path.empty() && frame - path.front().frame > keep) path.pop_front();
            while (!marks.empty() && frame - marks.front().frame > keep) marks.pop_front();
        }

        void clearOverlay() {
            if (overlay) {
                overlay->removeFromParentAndCleanup(true);
                overlay = nullptr;
            }
        }

        void reset() {
            clearOverlay();
            path.clear();
            marks.clear();
        }
    };

    State s_state;

    void showOverlay(PlayLayer* pl) {
        if (!pl->m_objectLayer) return;

        auto draw = CCDrawNode::create();

        // Trajectoire : plus ancienne = plus transparente
        if (Mod::get()->getSettingValue<bool>("show-path") && s_state.path.size() > 1) {
            auto n = s_state.path.size();
            for (size_t i = 1; i < n; i++) {
                float a = 0.15f + 0.85f * (static_cast<float>(i) / static_cast<float>(n));
                draw->drawSegment(
                    s_state.path[i - 1].pos, s_state.path[i].pos,
                    0.8f, ccc4f(1.f, 1.f, 1.f, a)
                );
            }
        }

        // Clics (vert) et relâchements (rouge)
        for (auto const& m : s_state.marks) {
            auto color = m.down ? ccc4f(0.2f, 1.f, 0.3f, 1.f)
                                : ccc4f(1.f, 0.25f, 0.25f, 1.f);
            draw->drawDot(m.pos, 4.f, color);
        }

        // Point de mort
        if (!s_state.path.empty()) {
            draw->drawDot(s_state.path.back().pos, 6.f, ccc4f(1.f, 0.9f, 0.1f, 1.f));
        }

        pl->m_objectLayer->addChild(draw, 9999);
        s_state.overlay = draw;
    }

    void recordInput(PlayerObject* self, bool down) {
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (self != pl->m_player1 && self != pl->m_player2) return;
        if (pl->m_player1->m_isDead) return;

        s_state.marks.push_back({ s_state.frame, self->getPosition(), down });
    }
}

class $modify(DIPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        s_state.reset();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (m_player1 && !m_player1->m_isDead) {
            s_state.frame++;
            s_state.path.push_back({ s_state.frame, m_player1->getPosition() });
            s_state.trim();
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        // m_isDead est faux si le noclip a annulé la mort
        if (m_player1 && m_player1->m_isDead && !s_state.overlay) {
            showOverlay(this);
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        s_state.reset();
    }
};

class $modify(DIPlayerObject, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        recordInput(this, true);
        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        recordInput(this, false);
        return PlayerObject::releaseButton(btn);
    }
};
