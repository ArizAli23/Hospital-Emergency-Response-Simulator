#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int NUM_BEDS = 5;    // number of hospital beds
const int MAX_WAITING = 8; // when 8 patients are waiting, one is treated automatically
const int MIN_PRIORITY = 1;
const int MAX_PRIORITY = 5;
const int AGING_MAX_PRIORITY = 4; // aging never raises a patient to 5 (Critical)

const int BED_SECONDS[6] = {0, 2, 3, 4, 5, 6};
const int MIN_ESTIMATE[6] = {0, 10, 25, 45, 2, 3};
const int MAX_ESTIMATE[6] = {0, 20, 40, 60, 3, 6};
const char *const ESTIMATE_UNIT[6] = {"", "minutes", "minutes", "minutes", "hours", "hours"};

long long getCurrentTime()
{
    return chrono::duration_cast<chrono::seconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
}

long long getCurrentTimeMs()
{
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
}

string formatTime(long long ts, const char *format = "%Y-%m-%d %H:%M:%S")
{
    time_t t = (time_t)ts;
    tm timeInfo;
#ifdef _WIN32
    localtime_s(&timeInfo, &t);
#else
    localtime_r(&t, &timeInfo);
#endif
    char buffer[40];
    strftime(buffer, sizeof(buffer), format, &timeInfo);
    return string(buffer);
}

string formatDuration(long long seconds) // "45s" or "3m 20s"
{
    if (seconds < 60)
        return to_string(seconds) + "s";
    return to_string(seconds / 60) + "m " + to_string(seconds % 60) + "s";
}

string toLower(string s)
{
    for (char &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

string trim(const string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool containsText(const string &text, const string &part)
{
    return toLower(text).find(toLower(part)) != string::npos;
}

bool isValidText(const string &text, bool allowDigits)
{
    string s = trim(text);
    if (s.empty() || s.size() > 30)
        return false;
    for (char ch : s)
    {
        unsigned char c = (unsigned char)ch;
        bool ok = isalpha(c) || c == ' ' || c == '-' || c == '.' || c == '\'';
        if (allowDigits && isdigit(c))
            ok = true;
        if (!ok)
            return false;
    }
    return true;
}

const char *priorityLabel(int p)
{
    const char *labels[] = {"Unknown", "Minor", "Mild", "Moderate", "Serious", "Critical"};
    return (p >= MIN_PRIORITY && p <= MAX_PRIORITY) ? labels[p] : labels[0];
}

struct Patient
{
    string id; // P001, P002, ...
    string name;
    int priority;
    int originalPriority; // priority before aging
    Patient *next;
    string treatmentType;
    long long time;     // arrival time
    long long lastAged; // when the priority was last raised

    Patient() : priority(0), originalPriority(0), next(nullptr), time(0), lastAged(0) {}
    Patient(int number, string Name, int Priority, string treatment, long long ts)
        : name(Name), priority(Priority), originalPriority(Priority), next(nullptr),
          treatmentType(treatment), time(ts), lastAged(ts)
    {
        id = to_string(number);
        while (id.size() < 3)
            id = "0" + id;
        id = "P" + id;
    }
};
class PriorityQueue
{
private:
    Patient *front;
    int count;      // number of waiting patients
    int nextNumber; // number for the next patient ID

    void insertNode(Patient *p)
    {
        if (isEmpty() || p->priority > front->priority)
        {
            p->next = front;
            front = p;
        }
        else
        {
            Patient *temp = front;
            while (temp->next && temp->next->priority >= p->priority)
            {
                temp = temp->next;
            }
            p->next = temp->next;
            temp->next = p;
        }
    }

    Patient *detach(const string &id)
    {
        string key = toLower(trim(id));
        Patient *prev = nullptr;
        for (Patient *cur = front; cur; prev = cur, cur = cur->next)
        {
            if (toLower(cur->id) == key)
            {
                if (prev)
                    prev->next = cur->next;
                else
                    front = cur->next;
                return cur;
            }
        }
        return nullptr;
    }

public:
    PriorityQueue() : front(nullptr), count(0), nextNumber(1) {}
    ~PriorityQueue() // frees the list (no memory leak)
    {
        while (front)
        {
            Patient *temp = front;
            front = front->next;
            delete temp;
        }
    }
    PriorityQueue(const PriorityQueue &) = delete;
    PriorityQueue &operator=(const PriorityQueue &) = delete;

    bool isEmpty() const { return front == nullptr; }
    int size() const { return count; }
    const Patient *getFront() const { return front; }

    Patient *enqueue(string name, int priority, string treatment)
    {
        Patient *newPatient = new Patient(nextNumber++, name, priority, treatment, getCurrentTime());
        insertNode(newPatient);
        count++;
        return newPatient;
    }

    bool dequeue(Patient &out)
    {
        if (isEmpty())
            return false;
        Patient *temp = front;
        front = front->next;
        out = *temp;
        out.next = nullptr;
        delete temp;
        count--;
        return true;
    }

    const Patient *findById(const string &id) const
    {
        string key = toLower(trim(id));
        for (const Patient *p = front; p; p = p->next)
            if (toLower(p->id) == key)
                return p;
        return nullptr;
    }

    vector<const Patient *> search(const string &text) const
    {
        vector<const Patient *> found;
        if (trim(text).empty())
            return found;
        for (const Patient *p = front; p; p = p->next)
            if (containsText(p->name, trim(text)) || containsText(p->id, trim(text)))
                found.push_back(p);
        return found;
    }

    bool remove(const string &id)
    {
        Patient *p = detach(id);
        if (!p)
            return false;
        delete p;
        count--;
        return true;
    }

    bool changePriority(const string &id, int newPriority)
    {
        Patient *p = detach(id);
        if (!p)
            return false;
        p->priority = newPriority;
        p->originalPriority = newPriority;
        p->lastAged = getCurrentTime();
        insertNode(p);
        return true;
    }

    void applyAging(int interval, vector<string> &messages)
    {
        long long now = getCurrentTime();
        vector<string> ids;
        for (const Patient *p = front; p; p = p->next)
            if (p->priority < AGING_MAX_PRIORITY && now - p->lastAged >= interval)
                ids.push_back(p->id);

        for (const string &id : ids)
        {
            Patient *p = detach(id);
            p->priority++;
            p->lastAged = now;
            insertNode(p);
            messages.push_back("Aging: " + p->name + " (" + p->id + ") waited too long, priority " +
                               to_string(p->priority - 1) + " -> " + to_string(p->priority) + ".");
        }
    }
};

struct Bed
{
    int number = 0;
    bool occupied = false;
    string patientId, patientName, condition, estimate;
    int priority = 0;
    long long startMs = 0;    // when the treatment started
    long long durationMs = 0; // how long the bed stays busy
};

class BedManager
{
private:
    vector<Bed> beds;

public:
    BedManager()
    {
        beds.resize(NUM_BEDS);
        for (int i = 0; i < NUM_BEDS; i++)
            beds[i].number = i + 1;
    }

    const vector<Bed> &all() const { return beds; }

    int findFree() const // index of the first free bed, or -1
    {
        for (int i = 0; i < NUM_BEDS; i++)
            if (!beds[i].occupied)
                return i;
        return -1;
    }

    int freeCount() const
    {
        int c = 0;
        for (const Bed &b : beds)
            if (!b.occupied)
                c++;
        return c;
    }

    const Bed *assign(const Patient &p)
    {
        int i = findFree();
        if (i < 0)
            return nullptr;
        int pr = p.priority;
        int shown = MIN_ESTIMATE[pr] + rand() % (MAX_ESTIMATE[pr] - MIN_ESTIMATE[pr] + 1);
        Bed &b = beds[i];
        b.occupied = true;
        b.patientId = p.id;
        b.patientName = p.name;
        b.condition = p.treatmentType;
        b.priority = pr;
        b.startMs = getCurrentTimeMs();
        b.durationMs = BED_SECONDS[pr] * 1000;
        b.estimate = to_string(shown) + " " + ESTIMATE_UNIT[pr];
        return &b;
    }

    vector<Bed> releaseFinished()
    {
        vector<Bed> done;
        for (Bed &b : beds)
        {
            if (b.occupied && getCurrentTimeMs() >= b.startMs + b.durationMs)
            {
                done.push_back(b);
                int number = b.number;
                b = Bed();
                b.number = number;
            }
        }
        return done;
    }
};
struct Stats
{
    int waiting = 0, freeBeds = 0;
    double avgWait = 0;
    long long longestWait = 0;
};

class Hospital
{
public:
    PriorityQueue queue;
    BedManager beds;
    vector<string> log; // activity messages
    string lastError;   // reason of the last failed action
    bool agingEnabled = true;
    int agingInterval = 60; // seconds of waiting for +1 priority

    Hospital() { srand((unsigned)time(nullptr)); }

    void note(const string &message)
    {
        log.push_back("[" + formatTime(getCurrentTime(), "%H:%M:%S") + "] " + message);
    }

    bool addPatient(string name, int priority, string type)
    {
        name = trim(name);
        type = trim(type);
        if (!isValidText(name, false))
            lastError = "Invalid name. Use letters, spaces, - . ' only (max 30 characters).";
        else if (!isValidText(type, true))
            lastError = "Invalid condition. Use letters, digits, spaces, - . ' only (max 30 characters).";
        else if (priority < MIN_PRIORITY || priority > MAX_PRIORITY)
            lastError = "Priority must be between 1 and 5.";
        else
            lastError.clear();
        if (!lastError.empty())
            return false;

        Patient *p = queue.enqueue(name, priority, type);
        note("Patient " + name + " (" + p->id + ") added with priority " + to_string(priority) + ".");

        if (queue.size() >= MAX_WAITING) // original rule: auto-treatment
        {
            note("Auto-Treatment taking place.");
            treatNext();
        }
        return true;
    }

    bool treatNext()
    {
        if (queue.isEmpty())
            lastError = "No patients in queue.";
        else if (beds.findFree() < 0)
            lastError = "All beds are occupied. Wait until a bed is free.";
        else
            lastError.clear();
        if (!lastError.empty())
        {
            note(lastError);
            return false;
        }

        Patient p;
        queue.dequeue(p);
        const Bed *bed = beds.assign(p);
        note("Patient " + p.name + " (" + p.id + ") is under treatment in Bed " + to_string(bed->number) +
             ". Estimated Time: " + bed->estimate + ".");
        return true;
    }

    void tick()
    {
        for (const Bed &b : beds.releaseFinished())
            note("Treatment completed for patient " + b.patientName + " (" + b.patientId + "). Bed " +
                 to_string(b.number) + " is free.");

        if (agingEnabled)
        {
            vector<string> messages;
            queue.applyAging(agingInterval, messages);
            for (const string &m : messages)
                note(m);
        }
    }

    bool removePatient(const string &id)
    {
        const Patient *p = queue.findById(id);
        if (!p)
        {
            lastError = "No waiting patient with ID '" + trim(id) + "'.";
            return false;
        }
        note("Patient " + p->name + " (" + p->id + ") was removed from the queue.");
        queue.remove(id);
        return true;
    }

    bool changePriority(const string &id, int newPriority)
    {
        const Patient *p = queue.findById(id);
        if (!p)
        {
            lastError = "No waiting patient with ID '" + trim(id) + "'.";
            return false;
        }
        note("Priority of " + p->name + " (" + p->id + ") changed " + to_string(p->priority) + " -> " +
             to_string(newPriority) + ".");
        queue.changePriority(id, newPriority);
        return true;
    }

    Stats getStats() const
    {
        Stats s;
        long long now = getCurrentTime(), total = 0;
        s.waiting = queue.size();
        s.freeBeds = beds.freeCount();
        for (const Patient *p = queue.getFront(); p; p = p->next)
        {
            total += now - p->time;
            if (now - p->time > s.longestWait)
                s.longestWait = now - p->time;
        }
        if (s.waiting > 0)
            s.avgWait = (double)total / s.waiting;
        return s;
    }

    void loadDemoData()
    {
        string names[] = {"Ali", "Sohail", "Ahmed", "Fiza", "Babar", "Khan"};
        string types[] = {"Covid", "Fever", "Cough", "Ankle Twist", "Injury", "Accident"};
        int priorities[] = {2, 1, 1, 3, 4, 5};
        for (int i = 0; i < 6; i++)
            addPatient(names[i], priorities[i], types[i]);
    }
};

const char *ansiColor(int p)
{
    const char *colors[] = {"\033[0m", "\033[36m", "\033[32m", "\033[33m", "\033[38;5;208m", "\033[1;31m"};
    return (p >= MIN_PRIORITY && p <= MAX_PRIORITY) ? colors[p] : colors[0];
}
const char *ansiReset() { return "\033[0m"; }

void enableAnsiColors()
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
}

void peek(const PriorityQueue &pq)
{
    const Patient *front = pq.getFront();
    if (!front)
    {
        cout << "No patients waiting.\n";
        return;
    }
    cout << "Next patient: " << ansiColor(front->priority) << front->name << " (" << front->id << ")"
         << ansiReset() << ", Type: " << front->treatmentType << " (Priority " << front->priority << ")\n";
}

void display(const PriorityQueue &pq)
{
    if (pq.isEmpty())
    {
        cout << "Queue is empty.\n";
        return;
    }
    long long now = getCurrentTime();
    bool anyAged = false;
    const string line(96, '-');
    cout << "\nCurrent Patient Queue:\n"
         << line << "\n";
    cout << left << setw(8) << "ID" << setw(20) << "Name" << setw(20) << "Condition"
         << setw(14) << "Priority" << setw(12) << "Waiting" << "Arrival Time\n";
    cout << line << "\n";
    for (const Patient *it = pq.getFront(); it; it = it->next)
    {
        string pri = to_string(it->priority) + " (" + priorityLabel(it->priority) + ")";
        if (it->priority != it->originalPriority)
        {
            pri += "*";
            anyAged = true;
        }
        cout << ansiColor(it->priority) << left << setw(8) << it->id << setw(20) << it->name
             << setw(20) << it->treatmentType << setw(14) << pri
             << setw(12) << formatDuration(now - it->time) << formatTime(it->time) << ansiReset() << "\n";
    }
    cout << line << "\n";
    if (anyAged)
        cout << "* priority was raised because the patient waited too long\n";
}

struct InputClosed
{
}; // thrown when the input ends (Ctrl+Z / Ctrl+D)

string readLine(const string &prompt)
{
    cout << prompt;
    string s;
    if (!getline(cin, s))
        throw InputClosed();
    return trim(s);
}

bool parseInt(const string &s, int &out)
{
    if (s.empty())
        return false;
    size_t pos = 0;
    try
    {
        out = stoi(s, &pos);
    }
    catch (...)
    {
        return false;
    }
    return pos == s.size();
}

int readInt(const string &prompt, int lo, int hi)
{
    while (true)
    {
        int v;
        if (parseInt(readLine(prompt), v) && v >= lo && v <= hi)
            return v;
        cout << "Invalid input. Enter a number from " << lo << " to " << hi << ".\n";
    }
}

string readValidText(const string &prompt, bool allowDigits, const string &errorMsg)
{
    while (true)
    {
        string s = readLine(prompt);
        if (isValidText(s, allowDigits))
            return s;
        cout << errorMsg << "\n";
    }
}

size_t printedLines = 0;
void printLog(const Hospital &h)
{
    while (printedLines < h.log.size())
        cout << "  " << h.log[printedLines++] << "\n";
}

void showMenu()
{
    cout << "\n--- HOSPITAL EMERGENCY RESPONSE SIMULATOR ---\n";
    cout << "1. Add Patient\n";
    cout << "2. Treat Next Patient (assign a bed)\n";
    cout << "3. Show Next Patient\n";
    cout << "4. Display All Patients\n";
    cout << "5. Show Bed Status\n";
    cout << "6. Search Patient\n";
    cout << "7. Remove Patient\n";
    cout << "8. Change Patient Priority\n";
    cout << "9. Statistics\n";
    cout << "0. Exit\n";
}

void showBeds(const Hospital &h)
{
    long long nowMs = getCurrentTimeMs();
    cout << "\nBed Status (" << NUM_BEDS - h.beds.freeCount() << "/" << NUM_BEDS << " occupied):\n";
    for (const Bed &b : h.beds.all())
    {
        if (!b.occupied)
        {
            cout << "\033[32mBed " << b.number << ": FREE\033[0m\n";
            continue;
        }
        double left = (b.startMs + b.durationMs - nowMs) / 1000.0;
        if (left < 0)
            left = 0;
        cout << ansiColor(b.priority) << "Bed " << b.number << ": " << b.patientName << " (" << b.patientId
             << "), " << b.condition << ", est. " << b.estimate
             << ", about " << fixed << setprecision(1) << left << " s left" << ansiReset() << "\n";
    }
}

void showStats(const Hospital &h)
{
    Stats s = h.getStats();
    cout << "\n=========== STATISTICS ===========\n";
    cout << "Patients waiting     : " << s.waiting << "\n";
    cout << "Beds occupied / free : " << NUM_BEDS - s.freeBeds << " / " << s.freeBeds << "\n";
    cout << "Average wait         : " << formatDuration((long long)s.avgWait) << "\n";
    cout << "Longest wait         : " << formatDuration(s.longestWait) << "\n";
    cout << "==================================\n";
}

void searchPatient(const Hospital &h)
{
    string q = readLine("Enter name or ID to search: ");
    if (q.empty())
    {
        cout << "Nothing entered.\n";
        return;
    }

    long long now = getCurrentTime();
    bool any = false;
    for (const Patient *p : h.queue.search(q))
    {
        cout << ansiColor(p->priority) << "  " << p->id << "  " << p->name << "  (" << p->treatmentType
             << ", priority " << p->priority << ", waiting " << formatDuration(now - p->time) << ")"
             << ansiReset() << "\n";
        any = true;
    }
    for (const Bed &b : h.beds.all())
        if (b.occupied && (containsText(b.patientId, q) || containsText(b.patientName, q)))
        {
            cout << "  " << b.patientId << "  " << b.patientName << "  is currently in Bed " << b.number << "\n";
            any = true;
        }
    if (!any)
        cout << "No patient found for '" << q << "'.\n";
}

int main()
{
    enableAnsiColors();
    Hospital h;
    h.loadDemoData();
    printLog(h);

    try
    {
        while (true)
        {
            h.tick();
            printLog(h);
            showMenu();

            int choice;
            if (!parseInt(readLine("Enter choice: "), choice))
                choice = -1;

            if (choice == 1)
            {
                string name = readValidText("Enter patient name: ", false,
                                            "Invalid name (letters, spaces, - . ' only), try again.");
                int priority = readInt("Enter priority (1=Minor, 5=Critical): ", MIN_PRIORITY, MAX_PRIORITY);
                string type = readValidText("Enter type: ", true,
                                            "Invalid type (letters, digits, spaces, - . ' only), try again.");
                if (!h.addPatient(name, priority, type))
                    cout << h.lastError << "\n";
                printLog(h);
            }
            else if (choice == 2)
            {
                bool ok = h.treatNext();
                printLog(h);
                if (ok)
                    peek(h.queue);
            }
            else if (choice == 3)
                peek(h.queue);
            else if (choice == 4)
                display(h.queue);
            else if (choice == 5)
                showBeds(h);
            else if (choice == 6)
                searchPatient(h);
            else if (choice == 7)
            {
                if (h.queue.isEmpty())
                {
                    cout << "No patients in queue.\n";
                    continue;
                }
                display(h.queue);
                string id = readLine("Enter patient ID to remove (e.g. P003): ");
                if (!h.removePatient(id))
                    cout << h.lastError << "\n";
                printLog(h);
            }
            else if (choice == 8)
            {
                if (h.queue.isEmpty())
                {
                    cout << "No patients in queue.\n";
                    continue;
                }
                display(h.queue);
                string id = readLine("Enter patient ID (e.g. P003): ");
                if (!h.queue.findById(id))
                {
                    cout << "No waiting patient with ID '" << id << "'.\n";
                    continue;
                }
                int np = readInt("Enter new priority (1=Minor, 5=Critical): ", MIN_PRIORITY, MAX_PRIORITY);
                if (!h.changePriority(id, np))
                    cout << h.lastError << "\n";
                printLog(h);
            }
            else if (choice == 9)
                showStats(h);
            else if (choice == 0)
                break;
            else
                cout << "Invalid choice.\n";
        }
    }
    catch (const InputClosed &)
    {
        cout << "\nInput closed.\n";
    }
    return 0;
}
