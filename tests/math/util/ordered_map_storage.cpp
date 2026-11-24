#include "support/folia_test.hpp"
import folia.math.util.ordered_map_storage;

namespace {
using namespace boost::ut;
using folia::math::OrderedMapStorage;

struct SameValueZeroNumber {
    bool operator()(double left,double right)const{
        return left==right||(std::isnan(left)&&std::isnan(right));
    }
};

suite ordered_map_storage=[] {
    "ordered_map_storage_same_value_zero_and_reinsert"_test=[] {
        OrderedMapStorage<double,int,SameValueZeroNumber> storage;
        const double nan=std::numeric_limits<double>::quiet_NaN();
        storage.set(-0.0,1);
        storage.set(+0.0,2);
        storage.set(nan,3);
        storage.set(std::numeric_limits<double>::signaling_NaN(),4);
        expect(storage.size()==2_u);
        expect(storage.slotCount()==2_u);
        expect(*storage.find(0.0)==2);
        expect(*storage.find(nan)==4);
        expect(storage.erase(-0.0));
        expect(storage.size()==1_u);
        expect(storage.slotCount()==2_u);
        storage.set(0.0,5);
        expect(storage.size()==2_u);
        expect(storage.slotCount()==3_u);
        const auto reinserted=storage.entryAt(2);
        expect(reinserted.has_value());
        expect(reinserted->key==0.0);
        expect(reinserted->value==5);
    };

    "ordered_map_storage_live_cursor_and_owner_release"_test=[] {
        using Owner=std::shared_ptr<int>;
        OrderedMapStorage<std::string,Owner> storage;
        auto retained=std::make_shared<int>(1);
        storage.set("first",retained);
        retained.reset();
        auto removed=std::make_shared<int>(2);
        std::weak_ptr<int> released=removed;
        storage.set("removed",removed);
        removed.reset();

        std::vector<std::string> visited;
        for(auto current=storage.begin(),finish=storage.end();current!=finish;++current){
            const auto entry=*current;
            visited.push_back(entry.key);
            if(entry.key=="first"){
                expect(storage.erase("removed"));
                expect(released.expired());
                storage.set("later",std::make_shared<int>(3));
            }
        }
        expect(visited.size()==2_u);
        expect(visited[0]=="first");
        expect(visited[1]=="later");
        storage.clear();
        expect(storage.size()==0_u);
        expect(!storage.entryAt(0));
    };

};
}
