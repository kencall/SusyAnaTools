#ifndef NTUPLE_READER_H
#define NTUPLE_READER_H

#include "SATException.h"

#include "TFile.h"
#include "TBranch.h"
#include "TTree.h"
#include "TLorentzVector.h"

#include <cstdio>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <set>
#include <typeinfo>
#include <typeindex>
#include <functional>
#include <cxxabi.h>
#include <iostream>

#include <iostream>

#ifdef __CINT__
#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

#pragma link C++ class vector<TLorentzVector>+;
#endif

#include <iostream>

/* This class is designed to be a simple interface to reading stop NTuples
   
   To use this class simply open the desired Tree as a TTree or TChain and pass it 
   to the constructor.  Then the tuple contents may be accessed as follows

   NTupleReader tr(tree);
   while(tr.getNextEvent())
   {
       const int& run = tr.getVar<unsigned int>("run");
   }

   and so on.  
 */

class NTupleReader;

void baselineUpdate(NTupleReader& tr);

class NTupleReader
{
    friend void baselineUpdate(NTupleReader& tr);

private:

    //Machinery to allow object cleanup
    //Generic deleter base class to obfiscate template type
    class deleter_base
    {
    public:
        virtual void destroy(void *) = 0;
        virtual ~deleter_base() {}
    };

    //Templated class to create/store simple object deleter 
    template<typename T> 
    class deleter : public deleter_base
    {
    public:
        virtual void destroy(void *ptr)
        {
            delete static_cast<T*>(ptr);
        }
    };

    //Templated class to create/store vector object deleter 
    template<typename T> 
    class vec_deleter : public deleter_base
    {
    public:
        virtual void destroy(void *ptr)
        {
            //Delete vector
            T *vecptr = *static_cast<T**>(ptr);
            if(vecptr != nullptr) delete vecptr;

            //depete pointer to vector
            delete static_cast<T*>(ptr);
        }
    };

    //Handle class to hold pointer and deleter
    class Handle
    {
    public:
        void* ptr;
        deleter_base* deleter;
        std::type_index type;

        Handle() : ptr(nullptr), deleter(nullptr), type(typeid(nullptr)) {}

        Handle(const Handle& h) : ptr(h.ptr), deleter(h.deleter), type(h.type) {}

        Handle(void* ptr, deleter_base* deleter = nullptr, const std::type_index& type = typeid(nullptr)) :  ptr(ptr), deleter(deleter), type(type) {}

        void destroy()
        {
            if(deleter)
            {
                deleter->destroy(ptr);
                delete deleter;
            }
        }
    };

    //Helper to make simple Handle
    template<typename T>
    static inline Handle createHandle(T* ptr)
    {
        return Handle(ptr, new deleter<T>, typeid(T));
    }

    //Helper to make vector Handle
    template<typename T>
    static inline Handle createVecHandle(T* ptr)
    {
        return Handle(ptr, new vec_deleter<T>, typeid(typename std::remove_pointer<T>::type));
    }

    //function wrapper 
    //base class
    class FuncWrapper
    {
    public:
        virtual bool operator()(NTupleReader& tr) = 0;
    };

    //class for arbitrary return value
    template<typename T>
    class FuncWrapperImpl : public FuncWrapper
    {
    private:
        T func_;
    public:
        bool operator()(NTupleReader& tr)
        {
            func_(tr);
            return true;
        }

        FuncWrapperImpl(T f) : func_(f) {}
    };

    template <class Tfrom, class Tto> 
    static void castVector(NTupleReader& tr, const std::string& var, const char typen);

    template <class Tfrom, class Tto> 
    static void castScalar(NTupleReader& tr, const std::string& var, const char typen);

    template <class Tfrom, class Tto> 
    static void addVectorAlias(NTupleReader& tr, const std::string& varFrom, const std::string& varAlias);

public:
    NTupleReader(TTree * tree, const std::set<std::string>& activeBranches_);
    NTupleReader(TTree * tree);
    NTupleReader();
    ~NTupleReader();

    std::string getFileName() const;

    int getEvtNum() const
    {
        return nevt_;
    }

    inline bool isFirstEvent() const
    {
        return evtProcessed_ <= 1;
    }

    int getNEntries() const;

    inline bool checkBranch(const std::string& name) const
    {
        return (typeMap_.find(name) != typeMap_.end());
    }
    inline bool hasVar(const std::string& name) const {return checkBranch(name); }

    bool goToEvent(int evt);
    bool getNextEvent();
    void disableUpdate();
    void printTupleMembers(FILE *f = stdout) const;

    void setConvertFloatingPointVectors(const bool doubleToFloat = true, const bool floatToDouble = false, const bool intToInt = false, const bool floatToInt = false);

    void setConvertFloatingPointScalars(const bool doubleToFloat = true, const bool floatToDouble = false, const bool intToFloat = true);

    template < typename Tfrom, typename Tto> void setVectorAlias(const std::string& varFrom, const std::string& varAlias);

    std::vector<std::string> getTupleMembers() const;
    std::vector<std::string> getTupleSpecs(const std::string& varName) const;

    template<typename T> void registerFunction(T f)
    {
        if(isFirstEvent()) functionVec_.emplace_back(new FuncWrapperImpl<T>(f));
        else THROW_SATEXCEPTION("New functions cannot be registered after tuple reading begins!\n");
    }

    //Specialization for basic functions
    void registerFunction(void (*f)(NTupleReader&));
    void registerFunction(bool (*f)(NTupleReader&));

    void getType(const std::string& name, std::string& type) const;

    void setReThrow(const bool);
    bool getReThrow() const;

    template<typename T> void registerDerivedVar(const std::string& name, T var)
    {
        try
        {
            auto handleItr = branchMap_.find(name);
            if(handleItr == branchMap_.end())
            {
                auto typeItr = typeMap_.find(name);
                if(typeItr != typeMap_.end())
                {
                    THROW_SATEXCEPTION("You are trying to redefine a tuple var: \"" + name + "\".  This is not allowed!  Please choose a unique name.");
                }
                handleItr = branchMap_.insert(std::make_pair(name, createHandle(new T()))).first;

                typeMap_[name] = demangle<T>();
            }
            setDerived(var, handleItr->second.ptr);
        }
        catch(const SATException& e)
        {
            e.print();
            if(reThrow_) throw;
        }
    }

    template<typename T> void registerDerivedVec(const std::string& name, T* var)
    {
        try
        {
            auto handleItr = branchVecMap_.find(name);
            if(handleItr == branchVecMap_.end())
            {
                auto typeItr = typeMap_.find(name);
                if(typeItr != typeMap_.end())
                {
                    THROW_SATEXCEPTION("You are trying to redefine a tuple var: \"" + name + "\".  This is not allowed!  Please choose a unique name.");
                }
                handleItr = branchVecMap_.insert(std::make_pair(name, createVecHandle(new T*()))).first;
            
                typeMap_[name] = demangle<T>();
            }
            T *vecptr = *static_cast<T**>(handleItr->second.ptr);
            if(vecptr != nullptr)
            {
                delete vecptr;
            }
            setDerived(var, handleItr->second.ptr);
        }
        catch(const SATException& e)
        {
            e.print();
            if(reThrow_) throw;
        }
    }

    void addAlias(const std::string& name, const std::string& alias);

    const void* getPtr(const std::string& var) const;
    const void* getVecPtr(const std::string& var) const;

    template<typename T> const T& getVar(const std::string& var, bool forceLoad = false) const
    {
        //This function can be used to return single variables

        try
        {
            return getTupleObj<T>(var, branchMap_, forceLoad);
        }
        catch(const SATException& e)
        {
            if(isFirstEvent()) e.print();
            if(reThrow_) throw;
            return *static_cast<T*>(nullptr);
        }
    }

    template<typename T> const std::vector<T>& getVec(const std::string& var, bool forceLoad = false) const
    {
        //This function can be used to return vectors

        try
        {
            return *getTupleObj<std::vector<T>*>(var, branchVecMap_, forceLoad);
        }
        catch(const SATException& e)
        {
            if(isFirstEvent()) e.print();
            if(reThrow_) throw;
            return *static_cast<std::vector<T>*>(nullptr);
        }
    }

    template<typename T, typename V> const std::map<T, V>& getMap(const std::string& var) const
    {
        //This function can be used to return maps

        try
        {
            return *getTupleObj<std::map<T, V>*>(var, branchVecMap_);
        }
        catch(const SATException& e)
        {
            if(isFirstEvent()) e.print();
            if(reThrow_) throw;
            return *static_cast<std::map<T, V>*>(nullptr);
        }
    }
 
    void setPrefix(std::string pre){
        prefix_ = pre;
    }
 
private:
    // private variables for internal use
    TTree *tree_;
    int nevt_, evtProcessed_;
    bool isUpdateDisabled_, reThrow_, convertHackActive_;

    std::string prefix_ = "";
    
    // stl collections to hold branch list and associated info
    mutable std::unordered_map<std::string, Handle> branchMap_;
    mutable std::unordered_map<std::string, Handle> branchVecMap_;
    std::vector<FuncWrapper*> functionVec_;
    mutable std::unordered_map<std::string, std::string> typeMap_;
    std::set<std::string> activeBranches_;

    void init();

    void setTree(TTree * tree);

    void populateBranchList();
    
    void registerBranch(TBranch * const branch) const;

    bool calculateDerivedVariables();

    bool goToEventInternal(int evt, const bool filter);

    template<typename T> void registerBranch(const std::string& name) const
    {
        branchMap_[name] = createHandle(new T());

        typeMap_[name] = demangle<T>();

        tree_->SetBranchStatus(name.c_str(), 1);
        tree_->SetBranchAddress(name.c_str(), branchMap_[name].ptr);
    }
    
    template<typename T> void registerVecBranch(const std::string& name) const
    {
        branchVecMap_[name] = createVecHandle(new std::vector<T>*());

        typeMap_[name] = demangle<std::vector<T>>();

        tree_->SetBranchStatus(name.c_str(), 1);
        tree_->SetBranchAddress(name.c_str(), branchVecMap_[name].ptr);
    }

    template<typename T> void updateTupleVar(const std::string& name, const T& var)
    {
        if(isFirstEvent())
        {
            if(branchMap_.find(name) == branchMap_.end())
            {
                branchMap_[name] = createVecHandle(new T());
                
                typeMap_[name] = demangle<T>();
            }
        }

        auto tuple_iter = branchMap_.find(name);
        if(tuple_iter != branchMap_.end())
        {
            *static_cast<T*>(tuple_iter->second.ptr) = var;
        }
        else THROW_SATEXCEPTION("Variable not found: \"" + name + "\"!!!\n");
    }

    template<typename T, typename V> T& getTupleObj(const std::string& var, const V& v_tuple, bool forceLoad = false) const
    {
        std::string varName;

        if(checkBranch(prefix_+var)){
            varName = prefix_+var;
        } else {
            varName = var;
        }

        //Find variable in the main tuple map 
        auto tuple_iter = v_tuple.find(varName);
        bool intuple = tuple_iter != v_tuple.end() ;

        //Check that the variable exists and the requested type matches the true variable type
        if(intuple && ( (tuple_iter->second.type == typeid(typename std::remove_pointer<T>::type)) || forceLoad ))
        {
            return *static_cast<T*>(tuple_iter->second.ptr);
        }
        else if(convertHackActive_ && intuple) //else check if it is a vector<float> or vector<double>
        {
            //hack to get vector<double> as vector<float>, requires DuplicateFDVector() to be run
            char typen;
            if( typeid(typename std::remove_pointer<T>::type) == typeid(std::vector<float>) && tuple_iter->second.type == typeid(std::vector<double>))
                typen='f';
            if( typeid(typename std::remove_pointer<T>::type) == typeid(std::vector<double>) && tuple_iter->second.type == typeid(std::vector<float>))
                typen='d';
            if( typeid(typename std::remove_pointer<T>::type) == typeid(std::vector<int>) && tuple_iter->second.type == typeid(std::vector<unsigned int>))
                typen='i';
            if( typeid(typename std::remove_pointer<T>::type) == typeid(std::vector<int>) && tuple_iter->second.type == typeid(std::vector<float>))
                typen='a';

            std::string newname = varName+"___" + typen;
            auto tuple_iter = branchVecMap_.find(newname);
            if (tuple_iter != branchVecMap_.end()){
                //std::cout << "Providing converted vector for " << varName << ", " << newname << std::endl;
                return *static_cast<T*>(tuple_iter->second.ptr);
            }

            std::string varType;
            getType(varName,varType);
            //std::cout << "Looking for doubles/floats for " << varName << " with type: " << varType << std::endl;

/*
            if( typeid(typename std::remove_pointer<T>::type) == typeid(float) && tuple_iter->second.type == typeid(double))
                typen='f';
            if( typeid(typename std::remove_pointer<T>::type) == typeid(double) && tuple_iter->second.type == typeid(float))
                typen='d';
            if( typeid(typename std::remove_pointer<T>::type) == typeid(float) && tuple_iter->second.type == typeid(int))
                typen='i';

            newname = varName+"___" + typen;
            auto tuple_iter2 = branchMap_.find(newname);
            if (tuple_iter2 != branchMap_.end())
                return *static_cast<T*>(tuple_iter2->second.ptr);
*/

            auto tuple_iter2 = branchMap_.find(varName+"___d");
            if (tuple_iter2 != branchMap_.end())
                return *static_cast<T*>(tuple_iter2->second.ptr);

            auto tuple_iter3 = branchMap_.find(varName+"___f");
            if (tuple_iter3 != branchMap_.end())
                return *static_cast<T*>(tuple_iter3->second.ptr);

            auto tuple_iter4 = branchMap_.find(varName+"___i");
            if (tuple_iter4 != branchMap_.end())
                return *static_cast<T*>(tuple_iter4->second.ptr);

        }
        else if( !intuple && (typeMap_.find(varName) != typeMap_.end())) //If it is not loaded, but is a branch in tuple
        {
            //If found in typeMap_, it can be added on the fly
            TBranch *branch = tree_->FindBranch(varName.c_str());
        
            //If branch not found continue on to throw exception
            if(branch != nullptr)
            {
                registerBranch(branch);
        
                //get iterator
                tuple_iter = v_tuple.find(varName);
        
                //force read just this branch
                branch->GetEvent(nevt_ - 1);
        
                intuple = true;

                //If it is the same type as requested, we can simply return the result
                if(tuple_iter->second.type == typeid(typename std::remove_pointer<T>::type))
                {
                    //return value
                    return *static_cast<T*>(tuple_iter->second.ptr);
                }
            }
        } 

        //It really does not exist, throw exception 
        auto typeIter = typeMap_.find(var);
        if(typeIter != typeMap_.end())
        {
            THROW_SATEXCEPTION("Variable not found: \"" + var + "\" with type \"" + demangle<typename std::remove_pointer<T>::type>() +"\", but is found with type \"" + typeIter->second + "\"!!!");
        }
        else
        {
            THROW_SATEXCEPTION("Variable not found: \"" + var + "\" with type \"" + demangle<typename std::remove_pointer<T>::type>() +"\"!!!");
        }
    }

    template<typename T> inline static void setDerived(const T& retval, void* const loc)
    {
        *static_cast<T*>(loc) = retval;
    }

    template<typename T> std::string demangle() const
    {
        // unmangled
        int status = 0;
        char* demangled = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
        std::string s = demangled;
        free(demangled);
        return s;
    }
};

#endif
