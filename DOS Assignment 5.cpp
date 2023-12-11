#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <map>
#include <chrono>
#include <utility>
#include <pthread.h>
#include <unistd.h>
#include <mutex>

using namespace std;

// Array of one-word philosopher names
const string one_word_philosophers[] = {
    "Thales", "Anaximander", "Anaximenes", "Pythagoras",
    "Heraclitus", "Parmenides", "Empedocles", "Anaxagoras",
    "Democritus", "Protagoras", "Gorgias", "Antisthenes",
    "Diogenes", "Plato", "Aristotle", "Speusippus",
    "Xenocrates", "Arcesilaus", "Carneades", "Epicurus",
    "Zeno", "Cleanthes", "Chrysippus", "Crates",
    "Pyrrho", "Timon", "Sextus", "Eudoxus",
    "Stilpo", "Cicero", "Plotinus", "Porphyry",
    "Iamblichus", "Proclus", "Simplicius", "Philoponus",
    "Damascius", "Socrates", "Hippasus", "Philolaus",
    "Eudoxus", "Archytas", "Alcmaeon", "Hippocrates",
    "Antiphon", "Metrodorus", "Leucippus", "Melissus",
    "Zeno", "Xenophanes", "Menippus", "Metrodorus",
    "Apollodorus", "Dio", "Aenesidemus", "Agrippa",
    "Aristo", "Bion", "Aristippus", "Arete",
    "Hegesias"
};

const int philosophers_size = sizeof(one_word_philosophers) / sizeof(one_word_philosophers[0]);

struct Bottle {
  int drinksRemaining = 5;
  bool beingRefilled = false;  // Indicates if the bottle is currently being refilled
  pthread_mutex_t mutex;       // Each bottle has its own mutex
  pthread_cond_t canRefill;    // Condition variable to wait on when the bottle is empty

  Bottle() {
    pthread_mutex_init(&mutex, NULL);         // Initialize the mutex
    pthread_cond_init(&canRefill, NULL);      // Initialize the condition variable
  }

  ~Bottle() {
    pthread_mutex_destroy(&mutex);            // Destroy the mutex
    pthread_cond_destroy(&canRefill);         // Destroy the condition variable
  }
};

struct Philosopher {
  int id;
  string name;
  vector<vector<pair<int, Bottle*>>> *graph; // Pointer to the graph
  map<int, string>* vertexNames; // Pointer to the map of vertex indices to names
};

struct BartenderTaskArg {
    vector<vector<pair<int, Bottle*>>> *graph;
    map<int, string>* vertexNames;
};

class ThreadSafeCout {
public:
  // Overload << operator to handle the locking and unlocking of the mutex along with output stream operations
  template<typename T>
  ThreadSafeCout& operator<<(const T& msg) {
      std::lock_guard<std::mutex> guard(_muCout); // Locks the mutex
      std::cout << msg; // Outputs the message
      return *this; // Returns the instance to allow chaining
  }

  // Specialization for std::ostream& (*)(std::ostream&) types, which includes std::endl
  ThreadSafeCout& operator<<(std::ostream& (*msg)(std::ostream&)) {
      std::lock_guard<std::mutex> guard(_muCout); // Locks the mutex
      std::cout << msg; // Outputs the message (e.g., std::endl)
      return *this; // Returns the instance to allow chaining
  }

private:
    static std::mutex _muCout; // Mutex for synchronizing access to std::cout
};

// Static member definition
std::mutex ThreadSafeCout::_muCout;

string timeSinceExecutionBegan() {
  static const auto begin = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  auto duration = currentTime - begin;
  auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  return "[" + std::to_string(nanoseconds) + " nanoseconds]: ";
}

// Function to get a random name from the array of philosopher names
string getRandomPhilosopherName() {
    return one_word_philosophers[rand() % philosophers_size];
}

// Function to generate a random undirected graph given the number of vertices
std::vector<std::vector<std::pair<int, Bottle*>>> generateRandomGraph(int numVertices, std::map<int, string>& vertexNames) {
    std::vector<std::vector<std::pair<int, Bottle*>>> graph(numVertices, std::vector<std::pair<int, Bottle*>>(numVertices, {0, nullptr}));

    // Assign a random name to each vertex
    for (int i = 0; i < numVertices; ++i) {
        vertexNames[i] = getRandomPhilosopherName(); // Get a random philosopher's name
    }

    // Generate random edges or ensure a single edge when only two vertices
    for (int i = 0; i < numVertices; ++i) {
        for (int j = i + 1; j < numVertices; ++j) {
            int isEdge;
            if (numVertices == 2) {
                // If there are only two philosophers, they must share an edge.
                isEdge = 1;
            } else {
                // Randomly decide if an edge should exist between vertex i and j
                isEdge = rand() % 2;
            }

            if (isEdge) {
                Bottle* sharedBottle = new Bottle(); // Create a new shared bottle

                graph[i][j] = {1, sharedBottle}; // Assign the shared bottle to the edge
                graph[j][i] = {1, sharedBottle}; // Mirror the edge for the undirected graph

                // Initialize the mutex for the shared bottle
                pthread_mutex_init(&sharedBottle->mutex, NULL);
            }
        }
    }

    return graph;
}

// Function to display the adjacency matrix along with vertex names
void displayAdjacencyMatrix(const std::vector<std::vector<std::pair<int, Bottle*>>>& graph, const std::map<int, string>& vertexNames) {
    int numVertices = graph.size();

    // Find the longest name length for alignment
    int maxNameLength = 0;
    for (const auto& namePair : vertexNames) {
        if (namePair.second.length() > maxNameLength) {
            maxNameLength = namePair.second.length();
        }
    }

    // Display the vertex names with alignment
    std::cout << std::string(maxNameLength + 3, ' '); // Offset for the row names
    for (int i = 0; i < numVertices; ++i) {
        std::cout << vertexNames.at(i) << std::string(maxNameLength - vertexNames.at(i).length() + 2, ' ');
    }
    std::cout << std::endl;

    // Display the adjacency matrix with aligned vertex names
    for (int i = 0; i < numVertices; ++i) {
        string name = vertexNames.at(i);
        std::cout << name << std::string(maxNameLength - name.length() + 2, ' ');
        for (int j = 0; j < numVertices; ++j) {
            std::cout << graph[i][j].first << std::string(maxNameLength, ' '); // Space between columns
        }
        std::cout << std::endl;
    }

  // Now, display the connections and the bottles for each vertex
  for (int i = 0; i < graph.size(); ++i) {
      std::cout << vertexNames.at(i) << " shares a bottle with: ";
      bool hasConnections = false;
      for (int j = 0; j < graph[i].size(); ++j) {
          if (graph[i][j].first) {
              std::cout << vertexNames.at(j) << ", ";
              hasConnections = true;
          }
      }
      if (!hasConnections) {
          std::cout << "No one";
      }
      std::cout << std::endl;
  }
}

void attemptToDrink(Philosopher* philosopher, int numberOfDrinks) {
    int id = philosopher->id;  // Get the philosopher's ID to access their connections in the graph.
    int drinksTaken = 0;
    std::map<int, string>& vertexNames = *philosopher->vertexNames;  // Assuming this map exists within the philosopher struct

    // Iterate over the philosopher's connections to find bottles with drinks.
  while(drinksTaken < numberOfDrinks) {
    for (int j = 0; j < (*philosopher->graph)[id].size(); ++j) {
      auto& connection = (*philosopher->graph)[id][j];
      if (connection.first && connection.second->drinksRemaining > 0) {
        string sharedWithPhilosopherName = vertexNames[j];
            
            pthread_mutex_lock(&(connection.second->mutex));  // Lock the bottle's mutex before accessing it.

            // Take the drinks from the bottle
            int drinksToTake = std::min(numberOfDrinks - drinksTaken, connection.second->drinksRemaining);
            connection.second->drinksRemaining -= drinksToTake;
            drinksTaken += drinksToTake;

            pthread_mutex_unlock(&(connection.second->mutex));  // Unlock the mutex after accessing the bottle.

            ThreadSafeCout() << timeSinceExecutionBegan() << philosopher->name << " took " << drinksToTake 
              << " drink(s) from bottle shared with philosopher " << sharedWithPhilosopherName << "." << std::endl;


            // Check if the philosopher has taken the required number of drinks.
            if (drinksTaken == numberOfDrinks) {
                break;  // Exit the loop if the philosopher has taken the required number of drinks.
            }
        }
    }
  }
}

void* bartenderTask(void* argument) {
    auto arg = *(BartenderTaskArg*)argument;
    auto graph = arg.graph;
    auto vertexNames = arg.vertexNames;

    while (true) {
        // Bartender refilling logic
        for (int i = 0; i < graph->size(); ++i) {
            for (int j = 0; j < (*graph)[i].size(); ++j) {
                auto& connection = (*graph)[i][j];
                if (connection.first) {
                    pthread_mutex_lock(&(connection.second->mutex));
                    if (connection.second->drinksRemaining <= 0 && !connection.second->beingRefilled) {
                        // Indicate that bottle is being refilled
                        connection.second->beingRefilled = true;
                        pthread_mutex_unlock(&(connection.second->mutex));

                        // Simulate refilling time
                        sleep(rand() % 3 + 1); // Sleep to simulate the time taken to refill the bottle

                        pthread_mutex_lock(&(connection.second->mutex));
                        // Refill the bottle
                        connection.second->drinksRemaining = 5;
                        connection.second->beingRefilled = false;
                        // Signal that the bottle has been refilled
                        pthread_cond_broadcast(&(connection.second->canRefill));

                        // Print out the message indicating the bottle has been refilled
                        ThreadSafeCout() << timeSinceExecutionBegan() << "Bottle shared between " 
                                         << (*vertexNames)[i] << " and " << (*vertexNames)[j]
                                         << " has been refilled." << std::endl;
                    }
                    pthread_mutex_unlock(&(connection.second->mutex));
                }
            }
        }
        sleep(1); // Bartender checks the bottles periodically
    }

    return nullptr;
}

void* philosopherTask(void* argument) {
    Philosopher* philosopher = (Philosopher*)argument;

    for (int i = 0; i < 10; ++i) {  // Loop 10 times
        ThreadSafeCout() << timeSinceExecutionBegan() << philosopher->name << " is tranquil." << endl;
        sleep(rand() % 5 + 1); // Sleep to simulate chillin
        int numberOfDrinks = rand() % 10 + 1;
        ThreadSafeCout() << timeSinceExecutionBegan() << philosopher->name << " is thirsty for " 
          << numberOfDrinks << " drinks." << endl;
        attemptToDrink(philosopher, numberOfDrinks);
    }
    ThreadSafeCout() << timeSinceExecutionBegan() << philosopher->name << " is tranquil." << endl;
    return nullptr;
}

int main() {
  srand((unsigned)time(0)); // Seed the random number generator

  int numVertices;
  cout << "Enter the number of philosophers: ";
  cin >> numVertices;

  map<int, string> vertexNames;
  vector<vector<pair<int, Bottle*>>> graph = generateRandomGraph(numVertices, vertexNames);
  cout << "Generated random undirected graph with named vertices:" << endl;
  displayAdjacencyMatrix(graph, vertexNames);

  // Create an array to store the thread ids and philosopher structs
  pthread_t threads[numVertices];
  Philosopher philosophers[numVertices]; // Array to keep track of philosophers

  // Create the bartender thread
  pthread_t bartenderThread;
  BartenderTaskArg bartenderArg{&graph, &vertexNames};
  if (pthread_create(&bartenderThread, NULL, bartenderTask, (void*)&bartenderArg)) {
      cerr << "Error creating bartender thread" << endl;
      return 1;
  }

  // Create a thread for each philosopher
  for (int i = 0; i < numVertices; ++i) {
      philosophers[i].id = i;
      philosophers[i].name = vertexNames[i]; // Assign philosopher's name
      philosophers[i].graph = &graph; // Pass the address of graph
      philosophers[i].vertexNames = &vertexNames; // Pass the address of the map
      if (pthread_create(&threads[i], NULL, philosopherTask, (void*)&philosophers[i])) {
          cerr << "Error creating thread for philosopher " << philosophers[i].name << endl;
          return 1;
      }
  }

  // Join philosopher threads
  for (int i = 0; i < numVertices; ++i) {
      if (pthread_join(threads[i], NULL)) {
          cerr << "Error joining thread for philosopher " << philosophers[i].name << endl;
          return 1;
      }
  }

  // pthread_join(bartenderThread, NULL);
  // This is not ideal and should be handled with a proper thread termination signal.
  pthread_detach(bartenderThread);

  return 0;
}
