#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <unordered_set>
#include <thread>

using namespace std;

unordered_set<int> usedIDs;

double random01() {
    static std::mt19937 rng(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    static std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

int generateUniqueID() {
    static std::mt19937 rng(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    static std::uniform_int_distribution<int> dist(100000, 999999);

    int id;
    do {
        id = dist(rng);
    } while (usedIDs.find(id) != usedIDs.end());

    usedIDs.insert(id);
    return id;
}

class Entity {
protected:
    int id;
    bool isAlive;
    int age;
    int energy;

public:
    Entity() {
        id = generateUniqueID();
        isAlive = true;
        age = 0;
        energy = 10;
    }

    void ageUp() {
        age++;
    }

    void die() {
        isAlive = false;
    }

    bool alive() const {
        return isAlive;
    }

    int getEnergy() const {
        return energy;
    }

    int getID() const {
        return id;
    }

    void addEnergy(int e) {
        energy += e;
    }

    void reduceEnergy(int e) {
        energy -= e;
        if (energy <= 0) die();
    }

    virtual void displayInfo() {
        cout << "ID: " << id << endl;
        cout << "Alive: " << (isAlive ? "Yes" : "No") << endl;
        cout << "Age: " << age << endl;
        cout << "Energy: " << energy << endl;
    }
};

class Plant : public Entity {
private:
    string type;
    int growthStage;

public:
    Plant(string t) : Entity() {
        type = t;
        growthStage = 0;
    }

    void grow(vector<Plant>& newPlants, int plantCount, string weather) {
        double growthChance = 0.3;
        if (weather == "Rain") growthChance = 0.5;
        if (weather == "Drought") growthChance = 0.1;

        if (isAlive && random01() < growthChance) growthStage++;

        if (growthStage > 5) {
            if (random01() < 0.01) die();
            else growthStage = 5;
        }

        double capFactor = 1.0 - (plantCount / 300.0);
        if (capFactor < 0) capFactor = 0;

        if (growthStage == 5 && random01() < (0.25 * capFactor)) {
            newPlants.push_back(Plant(type));
        }
    }

    void photosynthesize(string weather) {
        if (!isAlive) return;
        
        int boost = 2;
        if (weather == "Clear") boost = 3;
        if (weather == "Rain") boost = 1;
        if (weather == "Drought") boost = 0;

        energy += boost;
        if (energy > 30) energy = 30;
    }

    int beEaten(int amount) {
        if (!isAlive) return 0;
        int available = energy - 1; 
        if (available <= 0) return 0;

        int consumed = min(available, amount);
        energy -= consumed;
        return consumed;
    }

    void displayInfo() {
        Entity::displayInfo();
        cout << "Type: " << type << endl;
        cout << "Growth Stage: " << growthStage << endl;
    }
};

class Animal : public Entity {
private:
    string species;

public:
    Animal(string sp) : Entity() {
        species = sp;
    }

    void act(vector<Plant>& plants, vector<Animal>& newAnimals, int animalCount, string weather) {
        if (!isAlive) return;

        if (!plants.empty()) {
            int bestIndex = -1;
            int bestEnergy = -1;

            for (int i = 0; i < 5; i++) {
                int idx = (int)(random01() * plants.size());
                if (plants[idx].getEnergy() > bestEnergy) {
                    bestEnergy = plants[idx].getEnergy();
                    bestIndex = idx;
                }
            }

            if (bestIndex != -1) {
                int gained = plants[bestIndex].beEaten(5);
                energy += gained;
            }
        }

        int drain = 2;
        if (weather == "Drought") drain = 3;
        if (weather == "Rain") drain = 1;
        reduceEnergy(drain);

        double base = 0.05;
        double K = 300.0;
        double factor = 1.0 - (animalCount / K);
        if (factor < 0) factor = 0;

        double reproductionChance = base * factor;

        if (energy >= 20 && random01() < reproductionChance) {
            if (random01() < 0.25 && energy >= 40) {
                Animal c1(species);
                Animal c2(species);
                newAnimals.push_back(c1);
                newAnimals.push_back(c2);
                energy -= 30;
            } else {
                Animal child(species);
                newAnimals.push_back(child);
                energy -= 15;
            }
        }

        if (age > 30 && random01() < 0.1) die();
    }

    void displayInfo() {
        Entity::displayInfo();
        cout << "Species: " << species << " (Herbivore)" << endl;
    }
};

class Carnivore : public Entity {
private:
    string species;

public:
    Carnivore(string sp) : Entity() {
        species = sp;
        energy = 30;
    }

    void act(vector<Animal>& prey, vector<Carnivore>& newCarnivores, int carnCount, string weather) {
        if (!isAlive) return;

        if (!prey.empty() && energy < 40) {
            int targetIndex = (int)(random01() * prey.size());
            if (prey[targetIndex].alive()) {
                prey[targetIndex].die();
                energy += 25;
            }
        }

        int drain = 3;
        if (weather == "Drought") drain = 4;
        reduceEnergy(drain);

        double base = 0.02;
        double K = 50.0;
        double factor = 1.0 - (carnCount / K);
        if (factor < 0) factor = 0;

        if (energy >= 50 && random01() < (base * factor)) {
            Carnivore child(species);
            newCarnivores.push_back(child);
            energy -= 25;
        }

        if (age > 40 && random01() < 0.1) die();
    }

    void displayInfo() {
        Entity::displayInfo();
        cout << "Species: " << species << " (Carnivore)" << endl;
    }
};

class World {
public:
    vector<Plant> plants;
    vector<Animal> animals;
    vector<Carnivore> carnivores;
    int day = 0;
    string weather = "Clear";

    void step() {
        day++;

        double wChance = random01();
        if (wChance < 0.2) weather = "Rain";
        else if (wChance < 0.4) weather = "Drought";
        else weather = "Clear";

        vector<Plant> newPlants;
        for (int i = 0; i < plants.size(); i++) {
            plants[i].grow(newPlants, plants.size(), weather);
            plants[i].photosynthesize(weather);
        }
        for (int i = 0; i < newPlants.size(); i++) {
            plants.push_back(newPlants[i]);
        }

        vector<Animal> newAnimals;
        for (int i = 0; i < animals.size(); i++) {
            animals[i].ageUp();
            animals[i].act(plants, newAnimals, animals.size(), weather);
        }
        for (int i = 0; i < newAnimals.size(); i++) {
            animals.push_back(newAnimals[i]);
        }

        vector<Carnivore> newCarnivores;
        for (int i = 0; i < carnivores.size(); i++) {
            carnivores[i].ageUp();
            carnivores[i].act(animals, newCarnivores, carnivores.size(), weather);
        }
        for (int i = 0; i < newCarnivores.size(); i++) {
            carnivores.push_back(newCarnivores[i]);
        }

        for (int i = 0; i < plants.size(); ) {
            if (!plants[i].alive()) {
                plants[i] = plants.back();
                plants.pop_back();
            } else {
                i++;
            }
        }

        for (int i = 0; i < animals.size(); ) {
            if (!animals[i].alive()) {
                animals[i] = animals.back();
                animals.pop_back();
            } else {
                i++;
            }
        }

        for (int i = 0; i < carnivores.size(); ) {
            if (!carnivores[i].alive()) {
                carnivores[i] = carnivores.back();
                carnivores.pop_back();
            } else {
                i++;
            }
        }
    }

    int totalEnergy() {
        int sum = 0;
        for (int i = 0; i < plants.size(); i++) sum += plants[i].getEnergy();
        for (int i = 0; i < animals.size(); i++) sum += animals[i].getEnergy();
        for (int i = 0; i < carnivores.size(); i++) sum += carnivores[i].getEnergy();
        return sum;
    }

    void displayStats() {
        cout << "\n============================\n";
        cout << "Day: " << day << endl;
        cout << "Weather: " << weather << endl;
        cout << "Plants: " << plants.size() << endl;
        cout << "Herbivores: " << animals.size() << endl;
        cout << "Carnivores: " << carnivores.size() << endl;
        cout << "Total Energy: " << totalEnergy() << endl;
        cout << "============================\n";
    }
};

int main() {
    World world;

    for (int i = 0; i < 10; i++) {
        world.plants.push_back(Plant("Grass"));
    }

    while (true) {
        cout << "\033[2J\033[1;1H";
        world.displayStats();

        cout << "\n1. Advance 1 Day\n";
        cout << "2. Advance N Days\n";
        cout << "3. Add Herbivore\n";
        cout << "4. Add Carnivore\n";
        cout << "5. List Entities\n";
        cout << "6. Inspect Entity\n";
        cout << "7. Exit\n";
        cout << "Choice: ";

        int choice;
        if (!(cin >> choice)) {
            break; 
        }

        if (choice == 1) {
            world.step();
        }
        else if (choice == 2) {
            int n;
            cout << "Enter number of days: ";
            cin >> n;
            for (int i = 0; i < n; i++) {
                world.step();
                cout << "\033[2J\033[1;1H";
                world.displayStats();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        else if (choice == 3) {
            string sp;
            cout << "Species: ";
            cin >> sp;
            world.animals.push_back(Animal(sp));
        }
        else if (choice == 4) {
            string sp;
            cout << "Species: ";
            cin >> sp;
            world.carnivores.push_back(Carnivore(sp));
        }
        else if (choice == 5) {
            cout << "\033[2J\033[1;1H";
            cout << "--- Entity List ---\n";
            int listIndex = 1;

            for (int i = 0; i < world.animals.size(); i++) {
                cout << "[" << listIndex++ << "] Herbivore (" << world.animals[i].getID() << ")\n";
            }
            for (int i = 0; i < world.carnivores.size(); i++) {
                cout << "[" << listIndex++ << "] Carnivore (" << world.carnivores[i].getID() << ")\n";
            }
            for (int i = 0; i < world.plants.size(); i++) {
                cout << "[" << listIndex++ << "] Plant (" << world.plants[i].getID() << ")\n";
            }

            cout << "\nPress Enter to return...";
            cin.ignore(10000, '\n');
            cin.get();
        }
        else if (choice == 6) {
            int menuChoice;
            cout << "Enter Menu Index (from List Entities): ";
            cin >> menuChoice;

            cout << "\033[2J\033[1;1H";
            bool found = false;
            int currentIndex = 1;

            for (int i = 0; i < world.animals.size(); i++) {
                if (currentIndex == menuChoice) {
                    world.animals[i].displayInfo();
                    found = true;
                    break;
                }
                currentIndex++;
            }

            if (!found) {
                for (int i = 0; i < world.carnivores.size(); i++) {
                    if (currentIndex == menuChoice) {
                        world.carnivores[i].displayInfo();
                        found = true;
                        break;
                    }
                    currentIndex++;
                }
            }

            if (!found) {
                for (int i = 0; i < world.plants.size(); i++) {
                    if (currentIndex == menuChoice) {
                        world.plants[i].displayInfo();
                        found = true;
                        break;
                    }
                    currentIndex++;
                }
            }

            if (!found) cout << "Entity index not found.\n";

            cout << "\nPress Enter to return...";
            cin.ignore(10000, '\n');
            cin.get();
        }
        else if (choice == 7) {
            break;
        }
    }

    return 0;
}