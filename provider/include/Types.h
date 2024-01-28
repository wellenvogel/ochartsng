/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Types
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 */

#ifndef TYPES_H
#define TYPES_H
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <memory>

#define VAL(v) #v
#define TOSTRING(str) VAL(str)

typedef std::pair<std::string,std::string> NameValue;
typedef std::map<std::string,std::string> NameValueMap;
typedef std::string String;
typedef std::vector<String> StringVector;
typedef std::vector<uint8_t> DataVector;
typedef std::shared_ptr<DataVector> DataPtr;
namespace avnav{
template< typename ContainerT, typename PredicateT >
  int erase_if( ContainerT& items, const PredicateT& predicate ) {
    int rt=0;
    for( auto it = items.begin(); it != items.end(); ) {
      if( predicate(*it) ) {
        rt++;
        it = items.erase(it);
      }
      else ++it;
    }
    return rt;
  }

template< typename ContainerT, typename PredicateT >
  bool find_if( ContainerT& items, const PredicateT& predicate ) {
    for( auto it = items.begin(); it != items.end(); it++) {
      if( predicate(*it) ) return true;
    }
    return false;
  }

  template <typename T>
  class Guard{
    bool disabled=false;
    public:
    typedef T Callback;
    Callback callback=nullptr;
    Guard(Callback cb):callback(cb){}
    ~Guard(){
        if (! disabled) callback();
    }
    void disable(){
      disabled=true;
    }
  };
  typedef Guard<std::function<void(void)>> VoidGuard;
}


#endif /* TYPES_H */

