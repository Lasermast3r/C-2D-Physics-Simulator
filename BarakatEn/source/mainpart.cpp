#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>
#include <list>
#include <random>
#include <cstdint>
#include "Physics.hpp"

// --- HELPERS ---
bool LoadFont(sf::Font& font, std::string str) {
    if (!font.openFromFile(str)) return false;
    return true;
}

bool LoadSoundBuffer(sf::SoundBuffer& buffer, std::string locatefile, std::string str) {
    if (!buffer.loadFromFile(locatefile)) {
        if (!buffer.loadFromFile(str)) return false;
    }
    return true;
}

bool LoadTexture(sf::Texture& texture, std::string locatefile, std::string str) {
    if (!texture.loadFromFile(locatefile)) {
        if (!texture.loadFromFile(str)) return false;
    }
    texture.setSmooth(true);
    return true;
}

// --- STRUCTS ---
struct GameBox {
    sf::Sprite sprite;
    b2Body* body;
    float health;
    float maxHealth;

    GameBox(const sf::Texture& texture, float w, float h, float texturetype) : sprite(texture), body(nullptr) {
        float area = w * h;
        maxHealth = 100.0f + (area * 0.01f) + texturetype;
        health = maxHealth;
    }
};

struct BombObject {
    sf::Sprite sprite;
    b2Body* body;
    float health;
    float maxHealth;
    bool isArmed;
    float fuseTimer;

    BombObject(const sf::Texture& texture, float w, float h) : sprite(texture), body(nullptr) {
        maxHealth = 10.0f;
        health = maxHealth;
        isArmed = false;
        fuseTimer = 5.0f;
    }
};

class DamageListener : public b2ContactListener {
public:
    struct HitEvent {
        b2Body* body;
        float damage;
    };

    std::vector<HitEvent> pendingHits;
    std::vector<b2Body*> slingshotHits;
    b2Body* slingshotBody = nullptr;

    void BeginContact(b2Contact* contact) override {
        if (!slingshotBody) return;
        b2Body* bodyA = contact->GetFixtureA()->GetBody();
        b2Body* bodyB = contact->GetFixtureB()->GetBody();

        if (bodyA == slingshotBody) slingshotHits.push_back(bodyB);
        else if (bodyB == slingshotBody) slingshotHits.push_back(bodyA);
    }

    void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override {
        b2Body* bodyA = contact->GetFixtureA()->GetBody();
        b2Body* bodyB = contact->GetFixtureB()->GetBody();

        // Ignore slingshot collisions for damage
        if (bodyA == slingshotBody || bodyB == slingshotBody) return;

        float force = impulse->normalImpulses[0];
        if (force > 60.0f) {
            pendingHits.push_back({ contact->GetFixtureA()->GetBody(), force });
            pendingHits.push_back({ contact->GetFixtureB()->GetBody(), force });
        }
    }
};

// --- SOUND POOL ---
void PlaySound(sf::SoundBuffer& buffer, std::list<sf::Sound>& pool) {
    if (pool.size() > 16) return;
    pool.emplace_back(buffer);
    sf::Sound& s = pool.back();
    float pitch = 0.8f + (rand() % 40) / 100.0f;
    s.setPitch(pitch);
    s.setVolume(50.0f);
    s.play();
}

void Explode(b2Body* bombBody, std::vector<GameBox>& boxes, std::vector<BombObject>& bombs, sf::SoundBuffer& buffer, std::list<sf::Sound>& pool) {
    b2Vec2 bombPos = bombBody->GetPosition();
    float blastRadius = 10.0f;
    float blastForce = 400.0f;
    float blastDamage = 100.0f;

    PlaySound(buffer, pool);

    auto ApplyBlast = [&](b2Body* body, float& health) {
        b2Vec2 distanceVec = body->GetPosition() - bombPos;
        float distance = distanceVec.Length();
        if (distance < blastRadius) {
            float effect = 1.0f - (distance / blastRadius);
            health -= blastDamage * effect;
            distanceVec.Normalize();
            b2Vec2 impulse = (blastForce * effect) * distanceVec;
            body->ApplyLinearImpulse(impulse, body->GetWorldCenter(), true);
        }
        };

    for (auto& obj : boxes) ApplyBlast(obj.body, obj.health);
    for (auto& obj : bombs) {
        if (obj.body == bombBody) continue;
        obj.isArmed = true;
        ApplyBlast(obj.body, obj.health);
    }
}

// --- VISUALS ---
void SetSprite(sf::Sprite& sprite, const sf::Texture& texture, float width, float height, float posX, float posY) {
    sprite.setTexture(texture);
    sf::Vector2u imgSize = texture.getSize();
    sprite.setOrigin({ imgSize.x / 2.0f, imgSize.y / 2.0f });
    sprite.setPosition({ posX, posY });
    sprite.setScale({ width / imgSize.x, height / imgSize.y });
}

void SetupBoxSprite(sf::Sprite& sprite, const sf::Texture& texture, float width, float height, sf::Color colour) {
    sprite.setTexture(texture);
    sf::Vector2u size = texture.getSize();
    sprite.setOrigin({ (float)size.x / 2.0f, (float)size.y / 2.0f });
    sprite.setScale({ width / size.x, height / size.y });
    sprite.setColor(colour);
}

void SetText(sf::Text& text, std::string textvalue, float x, float y, bool center = true) {
    text.setString(textvalue);
    text.setCharacterSize(50);
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.0f,
        bounds.position.y + bounds.size.y / 2.0f
        });
    if (center) text.setPosition({ x / 2.0f, y / 2.0f });
    else text.setPosition({ x, y });
    text.setFillColor(sf::Color(0x6495EDFF));
    text.setOutlineThickness(1.0f);
    text.setOutlineColor(sf::Color::Red);
}

int main() {
    signed int windowWidth = 1200;
    float winWidCen = (float)windowWidth / 2.0f;
    signed int windowHeight = 750;
    float winHeiCen = (float)windowHeight / 2.0f;

    sf::RenderWindow window(sf::VideoMode({ (unsigned int)windowWidth, (unsigned int)windowHeight }), "Slingshot Bomb Game");
    window.setFramerateLimit(120);

    // --- RESOURCES ---
    sf::Font arial;
    if (!LoadFont(arial, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\FONTS\\Overseer.otf")) {
        if (!LoadFont(arial, "FONTS/Overseer.otf")) return -1;
    }

    sf::SoundBuffer deathBuffer, hurtBuffer, explodeBuffer, woodhitbuffer, wooddestroyedbuffer, stonehitbuffer, stonedestroyedbuffer;
    if (!LoadSoundBuffer(deathBuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\BoxDestroyed.mp3", "SOUND\\BoxDestroyed.mp3")) return -1;
    if (!LoadSoundBuffer(hurtBuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\HurtSound.mp3", "SOUND\\HurtSound.mp3")) return -1;
    LoadSoundBuffer(explodeBuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\Bomb_Explosion.mp3", "SOUND\\Bomb_Explosion.mp3");
    LoadSoundBuffer(woodhitbuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\Wood_Hit.mp3", "SOUND\\Wood_Hit.mp3");
    LoadSoundBuffer(wooddestroyedbuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\Wood_Destroy.mp3", "SOUND\\Wood_Destroyed.mp3");
    LoadSoundBuffer(stonedestroyedbuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\Rock_Destroyed.mp3", "SOUND\\Rock_Destroyed.mp3");
    LoadSoundBuffer(stonehitbuffer, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\SOUND\\Rock_Hit.mp3", "SOUND\\Rock_Hit.mp3");

    sf::Texture bgTexture, Crate, Stone, BombTex, Null,Slingshot_Texture;
    if (!bgTexture.loadFromFile("C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\Homes.jpeg")) {
        if (!LoadTexture(bgTexture, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\Homes.jpeg", "Images\\Homes.jpeg")) return -1;
    }
    LoadTexture(Crate, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\crate.jpg", "Images\\crate.jpg");
    LoadTexture(Stone, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\stone.png", "Images\\stone.png");
    LoadTexture(BombTex, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\Bomb.png", "Images\\Bomb.png");
    LoadTexture(Null, "C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\Null.jpg", "Images\\Null.jpg");
    LoadTexture(Slingshot_Texture,"C:\\Godot\\Projects\\Barakat\\BarakatEn\\build\\Images\\Slingshot.png","Images\\Slingshot.png");
    PhysicsWorld myPhysics;
    DamageListener damageListener;
    damageListener.pendingHits.reserve(100);
    myPhysics.world.SetContactListener(&damageListener);

    const sf::Texture* Obstacles[2] = { &Crate, &Stone };
    const sf::Color ObstaclesColors[6] = { sf::Color::Green, sf::Color::Blue, sf::Color::Magenta, sf::Color::Cyan, sf::Color::White, sf::Color::Yellow };

    // Walls
    myPhysics.CreateBox(windowWidth / 2.0f, windowHeight + 10.0f, windowWidth, 20.0f, false);
    myPhysics.CreateBox(windowWidth / 2.0f, -10.0f, windowWidth, 20.0f, false);
    myPhysics.CreateBox(-10.0f, windowHeight / 2.0f, 20.0f, windowHeight, false);
    myPhysics.CreateBox(windowWidth + 10.0f, windowHeight / 2.0f, 20.0f, windowHeight, false);

    sf::Sound hurtSound(hurtBuffer);
    sf::Sound deathSound(deathBuffer);
    sf::Sound explodeSound(explodeBuffer);

    sf::Sprite bgSprite(bgTexture);
    SetSprite(bgSprite, bgTexture, (float)windowWidth, (float)windowHeight, winWidCen, winHeiCen);

    // Slingshot Setup
    float slingX = 200.0f;
    float slingY = (float)windowHeight - 40.0f;
    sf::Sprite Slingshot(Slingshot_Texture);
    SetSprite(Slingshot, Slingshot_Texture, 40.0f, 80.0f, slingX, slingY);

    float collisionSize = 40.0f;
    b2Body* Slingshot_Collider = myPhysics.CreateBox(slingX, slingY - 20.0f, collisionSize, collisionSize, false);
    Slingshot_Collider->GetFixtureList()->SetSensor(true);
    damageListener.slingshotBody = Slingshot_Collider;

    sf::Text text(arial);
    sf::Text NoBoxUsed(arial);
    sf::Text BombText(arial);
    SetText(text, "LOL", (float)windowWidth, (float)windowHeight, true);
    SetText(NoBoxUsed, "Boxes: 0", 100.0f, 50.0f, false);
    SetText(BombText, "Bombs: 0", 100.0f, 100.0f, false);

    sf::FloatRect textBounds = text.getLocalBounds();
    b2Body* textBody = myPhysics.CreateBox(text.getPosition().x, text.getPosition().y, textBounds.size.x, textBounds.size.y, true);

    float skin;
    std::vector<GameBox> boxes;
    std::vector<BombObject> bombs;
    boxes.reserve(100);
    bombs.reserve(20);

    b2MouseJoint* mouseJoint = nullptr;
    b2Body* bodyBeingHeld = nullptr;
    b2DistanceJoint* slingSpring = nullptr;
    b2Body* BombSling = nullptr;
    const float SCALE = 30.0f;
    std::list<sf::Sound> soundPool;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    srand(seed);

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            // INPUT
            if (const auto* LMBPress = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                if (LMBPress->button == sf::Mouse::Button::Right) {
                    if (mouseJoint == nullptr) {
                        b2Body* bodyToGrab = nullptr;
                        if (text.getGlobalBounds().contains(mousePos)) bodyToGrab = textBody;
                        else {
                            for (int i = boxes.size() - 1; i >= 0; --i) {
                                if (boxes[i].sprite.getGlobalBounds().contains(mousePos)) { bodyToGrab = boxes[i].body; break; }
                            }
                            if (!bodyToGrab) {
                                for (int i = bombs.size() - 1; i >= 0; --i) {
                                    if (bombs[i].sprite.getGlobalBounds().contains(mousePos)) { bodyToGrab = bombs[i].body; break; }
                                }
                            }
                        }
                        if (bodyToGrab) {
                            mouseJoint = myPhysics.CreateMouseJoint(bodyToGrab, mousePos.x, mousePos.y);
                            bodyToGrab->SetAwake(true);
                            bodyBeingHeld = bodyToGrab;
                        }
                    }
                }
                else if (LMBPress->button == sf::Mouse::Button::Left) {
                    int Rand_Obstacle = rand() % 2;
                    int Rand_Colours = rand() % 6;
                    float w = 40.0f + (rand() % 60);
                    float h = 40.0f + (rand() % 60);
                    if (Obstacles[Rand_Obstacle] == &Stone) skin = 400.5f;
                    else skin = 100.3f;

                    GameBox newBox(*Obstacles[Rand_Obstacle], w, h, skin);
                    SetupBoxSprite(newBox.sprite, *Obstacles[Rand_Obstacle], w, w, ObstaclesColors[Rand_Colours]);
                    newBox.sprite.setPosition(mousePos);
                    newBox.body = myPhysics.CreateBox(mousePos.x, mousePos.y, w, w, true);
                    newBox.body->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
                    boxes.push_back(newBox);
                }
                else if (LMBPress->button == sf::Mouse::Button::Middle) {
                    bool deleted = false;
                    for (int i = boxes.size() - 1; i >= 0; --i) {
                        if (boxes[i].sprite.getGlobalBounds().contains(mousePos)) {
                            if (boxes[i].body == bodyBeingHeld && mouseJoint) { myPhysics.DestroyMouseJoint(mouseJoint); mouseJoint = nullptr; bodyBeingHeld = nullptr; }
                            myPhysics.world.DestroyBody(boxes[i].body);
                            boxes.erase(boxes.begin() + i);
                            deleted = true;
                            break;
                        }
                    }
                    if (!deleted) {
                        for (int i = bombs.size() - 1; i >= 0; --i) {
                            if (bombs[i].sprite.getGlobalBounds().contains(mousePos)) {
                                if (bombs[i].body == bodyBeingHeld && mouseJoint) { myPhysics.DestroyMouseJoint(mouseJoint); mouseJoint = nullptr; bodyBeingHeld = nullptr; }
                                myPhysics.world.DestroyBody(bombs[i].body);
                                bombs.erase(bombs.begin() + i);
                                break;
                            }
                        }
                    }
                }
            }
            // Press B to spawn a bomb
            if (const auto* keyPress = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPress->code == sf::Keyboard::Key::B) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    float w = 40.0f;
                    float h = 40.0f;

                    BombObject newBomb(BombTex, w, h);
                    SetupBoxSprite(newBomb.sprite, BombTex, w, h, sf::Color::White);
                    newBomb.sprite.setPosition(mousePos);
                    newBomb.body = myPhysics.CreateBox(mousePos.x, mousePos.y, w, h, true);
                    bombs.push_back(newBomb);
                }
            }

            if (const auto* LMBRelease = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (LMBRelease->button == sf::Mouse::Button::Right) {
                    if (mouseJoint) {
                        myPhysics.DestroyMouseJoint(mouseJoint);
                        mouseJoint = nullptr;
                        bodyBeingHeld = nullptr;

                        // --- LAUNCH LOGIC ---
                        if (slingSpring && BombSling) {
                            b2Vec2 anchorA = slingSpring->GetAnchorA();
                            b2Vec2 anchorB = slingSpring->GetAnchorB();
                            b2Vec2 distVec = anchorB - anchorA;
                            float distance = distVec.Length();

                            float threshold = 30.0f / SCALE;

                            if (distance > threshold) {
                                myPhysics.DestroySpring(slingSpring);
                                slingSpring = nullptr;

                                distVec.Normalize();
                                float launchForce = 100.0f;
                                BombSling->ApplyLinearImpulse(launchForce * distVec, BombSling->GetWorldCenter(), true);
                                BombSling = nullptr;
                            }
                            else {
                                myPhysics.DestroySpring(slingSpring);
                                slingSpring = nullptr;
                                BombSling = nullptr;
                            }
                        }
                    }
                }
            }
        }

        if (mouseJoint) {
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            myPhysics.UpdateMouseJoint(mouseJoint, mousePos.x, mousePos.y);
            if (bodyBeingHeld) bodyBeingHeld->SetAngularVelocity(0.0f);
        }

        myPhysics.Step();

        // --- CHECK SLINGSHOT LOADING ---
        for (b2Body* hitBody : damageListener.slingshotHits) {
            bool isBomb = false;
            for (auto& b : bombs) {
                if (b.body == hitBody) { isBomb = true; break; }
            }

            // FIX: Only load if we are HOLDING the bomb
            // This prevents the bomb from re-snapping to the sling as it flies out
            if (isBomb && !slingSpring && hitBody == bodyBeingHeld) {
                BombSling = hitBody;
                slingSpring = myPhysics.CreateSpring(hitBody, Slingshot_Collider);
            }
        }
        damageListener.slingshotHits.clear();

        // --- DAMAGE LOGIC ---
        bool hitSoundPlayed = false;

        for (const auto& hit : damageListener.pendingHits) {
            for (int i = boxes.size() - 1; i >= 0; --i) {
                if (boxes[i].body == hit.body) {
                    boxes[i].health -= hit.damage * 0.1f;
                    float hpPercent = boxes[i].health / boxes[i].maxHealth;
                    if (hpPercent < 0) hpPercent = 0;
                    std::uint8_t green = static_cast<std::uint8_t>(255 * hpPercent);
                    boxes[i].sprite.setColor(sf::Color(255, green, green));

                    if (boxes[i].health <= 0.0f) {
                        if (!hitSoundPlayed) {
                            if (&boxes[i].sprite.getTexture() == &Stone) PlaySound(stonedestroyedbuffer, soundPool);
                            else PlaySound(wooddestroyedbuffer, soundPool);
                            hitSoundPlayed = true;
                        }
                        if (boxes[i].body == bodyBeingHeld && mouseJoint) { myPhysics.DestroyMouseJoint(mouseJoint); mouseJoint = nullptr; bodyBeingHeld = nullptr; }
                        myPhysics.world.DestroyBody(boxes[i].body);
                        boxes.erase(boxes.begin() + i);
                    }
                    else if (!hitSoundPlayed) {
                        if (&boxes[i].sprite.getTexture() == &Stone) PlaySound(stonehitbuffer, soundPool);
                        else PlaySound(woodhitbuffer, soundPool);
                        hitSoundPlayed = true;
                    }
                    goto next_hit;
                }
            }
            for (int i = bombs.size() - 1; i >= 0; --i) {
                if (bombs[i].body == hit.body) {
                    if (!bombs[i].isArmed) bombs[i].isArmed = true;
                    bombs[i].health -= hit.damage * 0.1f;
                    if (bombs[i].health <= 0.0f) {
                        bombs[i].fuseTimer = 0.0f;
                    }
                    goto next_hit;
                }
            }
        next_hit:;
        }
        damageListener.pendingHits.clear();

        // --- BOMB TIMER ---sss
        float dt = 1.0f / 60.0f;
        for (int i = bombs.size() - 1; i >= 0; --i) {
            if (bombs[i].isArmed) {
                bombs[i].fuseTimer -= dt;
                int flash = (int)(bombs[i].fuseTimer * 10) % 2;
                if (flash == 0) bombs[i].sprite.setColor(sf::Color::Red);
                else bombs[i].sprite.setColor(sf::Color::White);

                if (bombs[i].fuseTimer <= 0.0f) {
                    Explode(bombs[i].body, boxes, bombs, explodeBuffer, soundPool);

                    if (bombs[i].body == BombSling && slingSpring) {
                        myPhysics.DestroySpring(slingSpring);
                        slingSpring = nullptr;
                        BombSling = nullptr;
                    }

                    if (bombs[i].body == bodyBeingHeld && mouseJoint) { myPhysics.DestroyMouseJoint(mouseJoint); mouseJoint = nullptr; bodyBeingHeld = nullptr; }
                    myPhysics.world.DestroyBody(bombs[i].body);
                    bombs.erase(bombs.begin() + i);
                }
            }
        }

        auto it = soundPool.begin();
        while (it != soundPool.end()) {
            if (it->getStatus() == sf::Sound::Status::Stopped) it = soundPool.erase(it);
            else ++it;
        }

        // --- RENDER ---
        b2Vec2 tPos = textBody->GetPosition();
        text.setPosition({ tPos.x * SCALE, tPos.y * SCALE });
        text.setRotation(sf::degrees(textBody->GetAngle() * 180.0f / 3.14159f));

        window.clear(sf::Color::Black);
        window.draw(bgSprite);
        window.draw(Slingshot);
        
        for (auto& box : boxes) {
            b2Vec2 bPos = box.body->GetPosition();
            box.sprite.setPosition({ bPos.x * SCALE, bPos.y * SCALE });
            box.sprite.setRotation(sf::degrees(box.body->GetAngle() * 180.0f / 3.14159f));
            window.draw(box.sprite);
        }
        for (auto& bomb : bombs) {
            b2Vec2 bPos = bomb.body->GetPosition();
            bomb.sprite.setPosition({ bPos.x * SCALE, bPos.y * SCALE });
            bomb.sprite.setRotation(sf::degrees(bomb.body->GetAngle() * 180.0f / 3.14159f));
            window.draw(bomb.sprite);
        }

        NoBoxUsed.setString("Boxes: " + std::to_string(boxes.size()));
        BombText.setString("Bombs: " + std::to_string(bombs.size()));
        
        window.draw(NoBoxUsed);
        window.draw(BombText);
        window.display();
    }
    return 0;
}