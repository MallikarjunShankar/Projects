#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <random>

const unsigned int WIN_WIDTH = 1200;
const unsigned int WIN_HEIGHT = 800;
const int WIN_SCORE = 100;
const float PLAYER_SPEED = 400.f;
const size_t MAX_PARTICLES = 50;

struct Player {
    sf::CircleShape shape;
    sf::Vector2f velocity{ 0.f, 0.f };
    float speed;
    int score = 0;

    struct Controls {
        sf::Keyboard::Key up, down, left, right;
    } keys;

    Player(sf::Color color, sf::Vector2f startPos, float moveSpeed, Controls c)
        : speed(moveSpeed), keys(c)
    {
        shape.setRadius(15.f);
        shape.setOrigin({ 15.f, 15.f });
        shape.setFillColor(color);
        shape.setPosition(startPos);
    }

    void handleInput() {
        velocity = { 0.f, 0.f };
        if (sf::Keyboard::isKeyPressed(keys.up)) velocity.y -= speed;
        if (sf::Keyboard::isKeyPressed(keys.down)) velocity.y += speed;
        if (sf::Keyboard::isKeyPressed(keys.left)) velocity.x -= speed;
        if (sf::Keyboard::isKeyPressed(keys.right)) velocity.x += speed;
    }

    void move(float dt, sf::Vector2u winSize) {
        sf::Vector2f pos = shape.getPosition() + velocity * dt;
        float r = shape.getRadius();
        pos.x = std::clamp(pos.x, r, static_cast<float>(winSize.x) - r);
        pos.y = std::clamp(pos.y, r, static_cast<float>(winSize.y) - r);
        shape.setPosition(pos);
    }
};

struct Particle {
    sf::CircleShape shape;
    Particle(sf::Vector2f pos) {
        shape.setRadius(4.f);
        shape.setOrigin({ 4.f, 4.f });
        shape.setFillColor(sf::Color::White);
        shape.setPosition(pos);
    }
};

bool checkCollision(const sf::Vector2f& a, float ra, const sf::Vector2f& b, float rb) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float d = dx * dx + dy * dy;
    float r = ra + rb;
    return d <= r * r;
}

int main() {
    sf::RenderWindow window(sf::VideoMode({ WIN_WIDTH, WIN_HEIGHT }), "Munch");
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if (!font.openFromFile("arial.ttf")) return EXIT_FAILURE;

    sf::SoundBuffer pickupBuf, winBuf;
    if (!pickupBuf.loadFromFile("pickup.wav")) return EXIT_FAILURE;
    if (!winBuf.loadFromFile("win.wav")) return EXIT_FAILURE;

    sf::Sound pickupSound(pickupBuf);
    sf::Sound winSound(winBuf);
    pickupSound.setVolume(60.f);
    winSound.setVolume(80.f);

    sf::Text scoreP1(font), scoreP2(font), winText(font);

    scoreP1.setCharacterSize(24);
    scoreP1.setFillColor(sf::Color::Blue);
    scoreP1.setPosition({ 20.f, 20.f });
    scoreP1.setString("P1: 0");

    scoreP2.setCharacterSize(24);
    scoreP2.setFillColor(sf::Color::Yellow);
    scoreP2.setString("P2: 0");
    sf::FloatRect b = scoreP2.getLocalBounds();
    scoreP2.setOrigin({ b.size.x, 0.f });
    scoreP2.setPosition({ WIN_WIDTH - 20.f, 20.f });

    winText.setCharacterSize(60);
    winText.setStyle(sf::Text::Bold);

    Player p1(sf::Color::Blue, { 100.f, WIN_HEIGHT / 2.f }, PLAYER_SPEED,
        { sf::Keyboard::Key::W, sf::Keyboard::Key::S, sf::Keyboard::Key::A, sf::Keyboard::Key::D });

    Player p2(sf::Color::Yellow, { WIN_WIDTH - 100.f, WIN_HEIGHT / 2.f }, PLAYER_SPEED,
        { sf::Keyboard::Key::Up, sf::Keyboard::Key::Down, sf::Keyboard::Key::Left, sf::Keyboard::Key::Right });

    std::vector<Particle> particles;
    particles.reserve(MAX_PARTICLES);

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> xDist(20.f, WIN_WIDTH - 20.f);
    std::uniform_real_distribution<float> yDist(20.f, WIN_HEIGHT - 20.f);
    std::uniform_real_distribution<float> spawnChance(0.f, 1.f);

    sf::Clock clock;
    bool gameOver = false;
    bool dirtyScore = false;

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            if (gameOver && event->is<sf::Event::KeyPressed>()) {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Space) {
                    p1.score = 0;
                    p2.score = 0;
                    p1.shape.setPosition({ 100.f, WIN_HEIGHT / 2.f });
                    p2.shape.setPosition({ WIN_WIDTH - 100.f, WIN_HEIGHT / 2.f });
                    particles.clear();
                    dirtyScore = true;
                    gameOver = false;
                }
            }
        }

        float dt = std::min(clock.restart().asSeconds(), 0.1f);

        if (!gameOver) {
            if (particles.size() < MAX_PARTICLES && spawnChance(rng) < 0.01f)
                particles.emplace_back(sf::Vector2f{ xDist(rng), yDist(rng) });

            p1.handleInput();
            p2.handleInput();
            p1.move(dt, window.getSize());
            p2.move(dt, window.getSize());

            if (checkCollision(p1.shape.getPosition(), p1.shape.getRadius(),
                p2.shape.getPosition(), p2.shape.getRadius())) {
                gameOver = true;
                winSound.play();
                if (p1.score > p2.score) {
                    winText.setString("Blue Wins!");
                    winText.setFillColor(sf::Color::Blue);
                }
                else if (p2.score > p1.score) {
                    winText.setString("Yellow Wins!");
                    winText.setFillColor(sf::Color::Yellow);
                }
                else {
                    winText.setString("Draw!");
                    winText.setFillColor(sf::Color::White);
                }
            }

            for (size_t i = 0; i < particles.size(); ) {
                bool collected = false;
                sf::Vector2f pos = particles[i].shape.getPosition();
                float r = particles[i].shape.getRadius();

                if (checkCollision(p1.shape.getPosition(), p1.shape.getRadius(), pos, r)) {
                    p1.score++;
                    collected = true;
                }
                else if (checkCollision(p2.shape.getPosition(), p2.shape.getRadius(), pos, r)) {
                    p2.score++;
                    collected = true;
                }

                if (collected) {
                    pickupSound.play();
                    dirtyScore = true;
                    particles[i] = particles.back();
                    particles.pop_back();
                }
                else {
                    ++i;
                }
            }

            if (p1.score >= WIN_SCORE || p2.score >= WIN_SCORE) {
                gameOver = true;
                winSound.play();
                if (p1.score >= WIN_SCORE) {
                    winText.setString("Blue Wins!");
                    winText.setFillColor(sf::Color::Blue);
                }
                else {
                    winText.setString("Yellow Wins!");
                    winText.setFillColor(sf::Color::Yellow);
                }
            }

            if (dirtyScore || gameOver) {
                scoreP1.setString("P1: " + std::to_string(p1.score));
                scoreP2.setString("P2: " + std::to_string(p2.score));
                sf::FloatRect b2 = scoreP2.getLocalBounds();
                scoreP2.setOrigin({ b2.size.x, 0.f });
                scoreP2.setPosition({ static_cast<float>(WIN_WIDTH) - 20.f, 20.f });

                if (gameOver) {
                    sf::FloatRect wb = winText.getLocalBounds();
                    winText.setOrigin({ wb.size.x / 2.f, wb.size.y / 2.f });
                    winText.setPosition({ WIN_WIDTH / 2.f, WIN_HEIGHT / 2.f });
                }
                dirtyScore = false;
            }
        }

        window.clear();
        for (const auto& p : particles) window.draw(p.shape);
        window.draw(p1.shape);
        window.draw(p2.shape);
        window.draw(scoreP1);
        window.draw(scoreP2);
        if (gameOver) window.draw(winText);
        window.display();
    }
}