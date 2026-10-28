#ifndef _TOPOLOGICALENTITY_H
#define _TOPOLOGICALENTITY_H


#include "rg_Const.h"

class TopologicalEntity
{
protected:
    rg_INT  m_ID;
    bool    m_delete;
    bool    m_flag_query;
    bool    m_flag_query_TOI;
public:
    //  constructor & deconstructor..
    TopologicalEntity();
    TopologicalEntity(const rg_INT& ID);
    TopologicalEntity(const TopologicalEntity& aTopoEntity);
    ~TopologicalEntity();

    //  get functions.. 
    inline rg_INT  getID() const { return m_ID; }
    inline bool     will_be_deleted() const {return m_delete;}
    inline bool    is_detected() const { return m_flag_query; }
    inline bool    is_detected_TOI() const { return m_flag_query_TOI; }

    //  set functions..
    void setID(const rg_INT& ID);
    void setTopologicalEntity(const TopologicalEntity& aTopoEntity);

    inline void will_be_deleted( const bool& deleteOrNot) { m_delete = deleteOrNot; }
    inline void is_detected( const bool& detectedOrNot ) { m_flag_query = detectedOrNot; }
    inline void is_detected_TOI( const bool& detectedOrNot ) { m_flag_query_TOI = detectedOrNot; }
    //  operator overloading..
    TopologicalEntity& operator =(const TopologicalEntity& aTopoEntity);
};

#endif
