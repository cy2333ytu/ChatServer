#ifndef USER_H
#define USER_H
#include <string>
namespace ccy
{

    class User
    {
    public:
        User(int Id = -1, std::string name = "", std::string pwd = "", std::string state = "offline")
        {
            this->Id = Id;
            this->name = name;
            this->password = pwd;
            this->state = state;
        }
        void setId(int Id) { this->Id = Id; }
        void setName(std::string name) { this->name = name; }
        void setPwd(std::string pwd) { this->password = pwd; }
        void setState(std::string state) { this->state = state; }

        int getId() const { return this->Id; }
        std::string getName() const { return this->name; }
        std::string getPwd() const { return this->password; }
        std::string getState() const { return this->state; }

    private:
        int Id;
        std::string name;
        std::string password;
        std::string state;
    };

}

#endif