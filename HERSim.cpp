#include <iostream>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <limits>

using namespace std;

string formatTime(long long ts) {
    time_t t = ts;
    tm timeInfo;

    localtime_s(&timeInfo, &t);

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);

    return string(buffer);
}

bool isAlphaOnly(string name){
    for(char c : name){
        if(!isalpha(c) && c != ' '){
            return false;
        }
    }
    return true;
}
long long getCurrentTime() {
    return chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
}

struct Patient
{
    static int noOfPatients;
    string name;
    int priority;
    Patient *next;
    string treatmentType;
    long long time;

    Patient(string Name, int Priority, string treatment, long long ts) : name(Name), priority(Priority), treatmentType(treatment), time(ts), next(nullptr){ noOfPatients ++;}
};

class PriorityQueue
{
private:
    Patient *front;
    void simulateTreatment(){
        int sleepTime;
        int maxTime, minTime;
        string unit;
        switch(front->priority){
            case 1:
                sleepTime = 2;
                maxTime = 20;
                minTime = 10;
                unit = "minutes";
                break;
            case 2:
                sleepTime = 3;
                maxTime = 40;
                minTime = 25;
                unit = "minutes";
                break;
            case 3:
                sleepTime = 4;
                maxTime = 60;
                minTime = 45;
                unit = "minutes";
                break;
            case 4:
                sleepTime = 5;
                maxTime = 3;
                minTime = 2;
                unit = "hours";
                break;
            case 5:
                sleepTime = 6;
                maxTime = 6;
                minTime = 3;
                unit = "hours";
                break;
        }
        int displayTime = minTime + rand() % (maxTime - minTime + 1);
        cout<< "Patient "<< front->name 
            <<" is currently under treatment. Estimated Time: "
            << displayTime<<" "<< unit<<endl; 
        this_thread::sleep_for(chrono::seconds(sleepTime));
        cout<<"Treatment completed for patient "<<front->name<<".\n";
    }
public:
    PriorityQueue() : front(nullptr){}
    bool isEmpty() { return front == nullptr; }

    void enqueue(string name, int priority, string treatment){
        Patient *newPatient = new Patient(name, priority, treatment, getCurrentTime());
        if (isEmpty() || priority > front->priority){
            newPatient->next = front;
            front = newPatient;
        }
        else{
            Patient *temp = front;
            while (temp->next && temp->next->priority >= priority){
                temp = temp->next;
            }
            newPatient->next = temp->next;
            temp->next = newPatient;
        }
        cout << "Patient " << name << " added with priority " << priority << ".\n";
        if(Patient::noOfPatients >=8){
            cout<<"Auto-Treatment taking place\n";
            dequeue();
        }
    }
    void dequeue(){
        if (isEmpty()){
            cout << "No patients in queue.\n";
            return;
        }
        simulateTreatment();
        Patient *temp = front;
        front = front->next;
        delete temp;
        Patient::noOfPatients--;
        peek();
    }

    void peek(){
        if (isEmpty()){
            cout << "No patients waiting.\n";
            return;
        }
        cout << "Next patient: " << front->name
             << ", Type: " << front->treatmentType
             << " (Priority " << front->priority << ")\n";
    }
    void display(){
        if (isEmpty()){
            cout << "Queue is empty.\n";
            return;
        }
        cout << "\nCurrent Patient Queue:\n";
        cout << "-------------------------------------------------------------------------------------\n";
        cout << left << setw(15) << "Name" << setw(15) << "Condition" << setw(15) << "Priority" << setw(25) << "Time" <<endl;
        cout << "-------------------------------------------------------------------------------------\n";
        Patient *iterator = front;
        while (iterator){
            cout<< left << setw(15) << iterator->name 
                << setw(15) << iterator->treatmentType 
                << setw(15) << iterator->priority 
                << setw(25) << formatTime(iterator->time)<<endl;
            iterator = iterator->next;
        }
        cout << "-------------------------------------------------------------------------------------\n";
    }
};
int Patient::noOfPatients = 0;
int main()
{
    PriorityQueue pq;
    int choice, priority;
    string name, type;
    string names[] = {"Ali", "Sohail", "Ahmed", "Fiza", "Babar", "Khan"};
    string types[] = {"Covid", "Fever", "Cough", "Ankle Twist", "Injury", "Accident"};
    int priorities[] = {2, 1, 1, 3, 4, 5};
    int autoCount = 6;

    for (int i = 0; i < autoCount; i++) {
        pq.enqueue(names[i], priorities[i], types[i]);
    }
    while (true)
    {
        cout << "\n--- HOSPITAL PRIORITY QUEUE ---\n";
        cout << "1. Add Patient\n";
        cout << "2. Treat Next Patient\n";
        cout << "3. Show Next Patient\n";
        cout << "4. Display All Patients\n";
        cout << "5. Return to Main Menu\n";
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 1){
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            do{
            cout << "Enter patient name: ";
            getline(cin, name);

                if (name.empty() || !isAlphaOnly(name)) {
                cout << "Invalid name, try again.\n";
                }
            } while (name.empty() || !isAlphaOnly(name));
            cout << "Enter priority (1=Minor, 5=Critical): ";
            cin >> priority;
            while(cin.fail() || priority < 1 || priority > 5){
                cin.clear();
                cin.ignore(1000, '\n');
                cout<<"Invalid Priority, Try Again: ";
                cin >> priority;
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            do{
                cout << "Enter type: ";
                getline(cin, type);
                if(type.empty() || !isAlphaOnly(type)){
                    cout << "Invalid type, try again.\n";
                }
            } while(type.empty() || !isAlphaOnly(type));
            pq.enqueue(name, priority, type);
        }
        else if (choice == 2)
            pq.dequeue();
        else if (choice == 3)
            pq.peek();
        else if (choice == 4)
            pq.display();
        else if (choice == 5)
            break;
        else
            cout << "Invalid choice.\n";
    }
    return 0;
}

