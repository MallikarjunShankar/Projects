#include <iostream>
#include <vector>
#include <SFML/Graphics.hpp>
#include <string>
#include <cmath>
#include <optional>
#include <deque>
#include <corecrt_math_defines.h>

// Physics Constants
// G is normalized to 4*PI^2 so that:
// Mass is in Solar Masses, Distance in AU (Astronomical Units), Time in Years.
// This prevents floating-point underflow errors common with SI units (e.g., 6.67e-11).
const double G = 4 * M_PI * M_PI;
const double dt = 0.0005; // Time step (simulation speed/precision trade-off)
const double EPS = 1e-10; // Epsilon to prevent division by zero in collisions

// Custom 3D Vector structure using double precision.
// 'double' is required for orbital mechanics; 'float' lacks the precision
// needed to maintain stable orbits over long simulation times.
struct Vec3 {
    double x, y, z;
};

// Operator overloads for clean vector math syntax (e.g., v1 + v2)
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
inline Vec3 operator*(const Vec3& a, double scalar) {
    return { a.x * scalar, a.y * scalar, a.z * scalar };
}
inline Vec3 operator*(double scalar, const Vec3& a) {
    return { a.x * scalar, a.y * scalar, a.z * scalar };
}
inline Vec3 operator/(const Vec3& a, double scalar) {
    return { a.x / scalar, a.y / scalar, a.z / scalar };
}
inline double norm(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

struct Body {
    double mass;
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;
    sf::CircleShape shape;
    // Deque used for trails to allow efficient removal from the front (oldest points)
    std::deque<sf::Vector2f> trail;
};

// N-Body Force Calculation: O(N^2) complexity
// Computes gravitational force between every pair of bodies.
void computeAccelerations(std::vector<Body>& bodies) {
    // Reset accelerations
    for (auto& b : bodies)
        b.acceleration = { 0,0,0 };

    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            Vec3 r = bodies[j].position - bodies[i].position;

            double dist = norm(r) + EPS;
            double invDist3 = 1.0 / (dist * dist * dist);

            // Newton's Law of Universal Gravitation: F = G * m1 * m2 / r^2
            // Vector form: F_vec = F * (r_vec / r)
            Vec3 force = r * (G * invDist3);

            // Apply forces (Action-Reaction pair)
            bodies[i].acceleration =
                bodies[i].acceleration + bodies[j].mass * force;

            bodies[j].acceleration =
                bodies[j].acceleration - bodies[i].mass * force;
        }
    }
}

// Velocity Verlet Integration
// A symplectic integrator that conserves energy better than Euler integration.
// Essential for orbital stability.
void velocityVerlet(std::vector<Body>& bodies) {
    static std::vector<Vec3> oldAcc;
    oldAcc.resize(bodies.size());

    // Step 1: Update Position and Half-Update Velocity
    for (size_t i = 0; i < bodies.size(); ++i) {
        oldAcc[i] = bodies[i].acceleration;

        bodies[i].position =
            bodies[i].position +
            bodies[i].velocity * dt +
            bodies[i].acceleration * (0.5 * dt * dt);
    }

    // Step 2: Compute new forces based on new positions
    computeAccelerations(bodies);

    // Step 3: Finish updating Velocity using average of old and new acceleration
    for (size_t i = 0; i < bodies.size(); ++i) {
        bodies[i].velocity =
            bodies[i].velocity +
            (oldAcc[i] + bodies[i].acceleration) * (0.5 * dt);
    }
}

// Global scaling factors for rendering
float SCALE;
sf::Vector2f CENTER;

// Converts simulation coordinates (AU) to screen coordinates (Pixels)
sf::Vector2f toScreen(const Vec3& p) {
    return sf::Vector2f(
        CENTER.x + static_cast<float>(p.x * SCALE),
        CENTER.y + static_cast<float>(p.y * SCALE)
    );
}

// Factory function to create planets with initial circular orbital velocity
Body makePlanet(double mass, double radius, sf::Color color, float drawSize) {
    Body b;

    b.mass = mass;
    b.position = { radius,0,0 };

    // Vis-viva equation simplification for circular orbit: v = sqrt(GM/r)
    // Assumes orbiting a central body of mass 1.0 (The Sun)
    double v = std::sqrt(G / radius);
    b.velocity = { 0,v,0 };

    b.shape = sf::CircleShape(drawSize);
    b.shape.setFillColor(color);
    b.shape.setOrigin({ drawSize, drawSize });

    return b;
}

// Corrects system momentum so the Solar System doesn't drift off-screen.
// Centers the simulation on the Barycenter (Center of Mass).
void enforceBarycenter(std::vector<Body>& bodies)
{
    Vec3 totalMomentum{ 0,0,0 };
    double totalMass = 0.0;

    for (const auto& b : bodies) {
        totalMomentum = totalMomentum + b.mass * b.velocity;
        totalMass += b.mass;
    }

    Vec3 correction = totalMomentum / totalMass;

    for (auto& b : bodies)
        b.velocity = b.velocity - correction;
}

int main()
{
    auto desktop = sf::VideoMode::getDesktopMode();

    sf::RenderWindow window(
        desktop,
        "Solar System N-Body Simulation",
        sf::Style::Default
    );

    // Center origin on screen
    CENTER = sf::Vector2f(
        window.getSize().x * 0.5f,
        window.getSize().y * 0.5f
    );

    // Determine scale: Fit orbit of Neptune (approx 30 AU) into the screen
    float maxOrbitAU = 30.1f;
    float usableRadius =
        std::min(window.getSize().x,
            window.getSize().y) * 0.45f;

    SCALE = usableRadius / maxOrbitAU;

    window.setFramerateLimit(60);

    // Simulation Setup
    std::vector<Body> bodies;

    // The Sun
    Body sun;
    sun.mass = 1.0;
    sun.position = { 0,0,0 };
    sun.velocity = { 0,0,0 };
    sun.shape = sf::CircleShape(4.f);
    sun.shape.setFillColor(sf::Color::Yellow);
    sun.shape.setOrigin({ 4.f,4.f });

    bodies.push_back(sun);

    // Planets (Mass relative to Sun, Distance in AU)
    bodies.push_back(makePlanet(1.66e-7, 0.39, sf::Color(200, 200, 200), 1.f)); // Mercury
    bodies.push_back(makePlanet(2.45e-6, 0.72, sf::Color(255, 180, 120), 1.5f)); // Venus
    bodies.push_back(makePlanet(3.00e-6, 1.00, sf::Color::Blue, 2.f));           // Earth
    bodies.push_back(makePlanet(3.23e-7, 1.52, sf::Color::Red, 1.5f));            // Mars
    bodies.push_back(makePlanet(9.54e-4, 5.20, sf::Color(210, 170, 120), 3.f));   // Jupiter
    bodies.push_back(makePlanet(2.86e-4, 9.58, sf::Color(220, 200, 150), 2.5f));  // Saturn
    bodies.push_back(makePlanet(4.36e-5, 19.2, sf::Color::Cyan, 2.f));            // Uranus
    bodies.push_back(makePlanet(5.15e-5, 30.1, sf::Color(120, 120, 255), 2.f));   // Neptune

    computeAccelerations(bodies);
    enforceBarycenter(bodies); // Stabilize system momentum before starting

    const size_t MAX_TRAIL = 1500;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // Sub-stepping: Perform 5 physics updates per frame for smoother orbits
        for (int i = 0; i < 5; ++i)
            velocityVerlet(bodies);

        // Trail Management
        for (auto& b : bodies)
        {
            b.trail.push_back(toScreen(b.position));
            if (b.trail.size() > MAX_TRAIL)
                b.trail.pop_front();
        }

        window.clear(sf::Color::Black);

        // Render Trails
        for (auto& b : bodies)
        {
            if (b.trail.size() > 1)
            {
                // Create a vertex array for the trail
                sf::VertexArray trail(sf::PrimitiveType::LineStrip, b.trail.size());

                for (size_t i = 0; i < b.trail.size(); ++i)
                {
                    // Calculate alpha fade based on index (older points = more transparent)
                    float alpha = static_cast<float>(i) / b.trail.size();
                    trail[i].position = b.trail[i];
                    trail[i].color = sf::Color(
                        b.shape.getFillColor().r,
                        b.shape.getFillColor().g,
                        b.shape.getFillColor().b,
                        static_cast<std::uint8_t>(alpha * 255)
                    );
                }

                window.draw(trail);
            }
        }

        // Render Bodies
        for (auto& b : bodies)
        {
            b.shape.setPosition(toScreen(b.position));
            window.draw(b.shape);
        }

        window.display();
    }

    return 0;
}
