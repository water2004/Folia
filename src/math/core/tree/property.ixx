// Native storage adaptation for MathJax core/Tree/Node.ts PropertyList.
// MathJax source is Apache-2.0; see docs/math/architecture.md for provenance.
export module folia.math.core.tree.property;
export import folia.math.util.callable_types;
import std;
import folia.math.util.option_types;

export namespace folia::math {
struct PropertyNull { bool operator==(const PropertyNull&) const = default; };
// Object.prototype values are observable on malformed attribute names, but are
// not callable math services. Keep their identities, rather than stringify them.
enum class BuiltinProperty {
    constructor, defineGetter, defineSetter, hasOwnProperty, lookupGetter,
    lookupSetter, isPrototypeOf, propertyIsEnumerable, toString, valueOf,
    toLocaleString, prototype, arrayConstructor, arrayToString, arrayJoin, arrayPrototype,
    numberPrototype, numberConstructor, numberToString, numberValueOf,
    functionPrototype, functionConstructor, functionCall, functionApply,
    functionBind, functionToString
};
struct PropertyStorage;
class PropertyList;
class PropertyArray;
struct Property {
    using Value = std::variant<Missing, PropertyNull, bool, double, std::string, BuiltinProperty,
                               std::shared_ptr<PropertyStorage>, FunctionRef, ContextFunctionIdentity,
                               options::SymbolRef>;
    Value value = Missing{};
    Property() = default;
    Property(Missing v) : value(v) {}
    Property(PropertyNull v) : value(v) {}
    Property(bool v) : value(v) {}
    Property(double v) : value(v) {}
    Property(int v) : value(double(v)) {}
    Property(const char* v) : value(std::string(v)) {}
    Property(std::string v) : value(std::move(v)) {}
    Property(BuiltinProperty v) : value(v) {}
    Property(options::SymbolRef v) : value(v) {}
    Property(FunctionRef v) : value(v) {
        if(!v)throw options::Exception("TypeError",u"Native callable property is null");
    }
    Property(ContextFunctionIdentity v):value(std::move(v)){}
    Property(const PropertyList& v);
    Property(const PropertyArray& v);
    bool missing() const { return std::holds_alternative<Missing>(value); }
    bool operator==(const Property&) const;
};
// A view of an existing record graph. Bound storage owns no parallel entries:
// every operation reaches the original record, including getters and setters.
struct PropertyRecordBinding {
    virtual ~PropertyRecordBinding()=default;
    virtual Property get(std::string_view)=0;
    virtual Property getOwn(std::string_view)=0;
    virtual bool has(std::string_view)const=0;
    virtual bool hasOwn(std::string_view)const=0;
    // Descriptor metadata must be inspectable without retrieving the value.
    // Most typed owners expose only enumerable own properties; Context-backed
    // records override these methods to preserve the current descriptor flags.
    virtual bool ownPropertyEnumerable(std::string_view name)const{return hasOwn(name);}
    virtual void set(std::string_view,Property)=0;
    virtual void setOwn(std::string_view,Property)=0;
    virtual void erase(std::string_view)=0;
    // Symbol properties are part of the same record owner. These methods are
    // separate from string-key enumeration (Object.keys) and preserve the
    // exact Symbol identity used by Reflect/assign operations.
    virtual Property getSymbol(options::SymbolRef){return Missing{};}
    virtual Property getOwnSymbol(options::SymbolRef){return Missing{};}
    virtual bool hasSymbol(options::SymbolRef)const{return false;}
    virtual bool hasOwnSymbol(options::SymbolRef)const{return false;}
    virtual bool ownSymbolEnumerable(options::SymbolRef symbol)const{return hasOwnSymbol(symbol);}
    virtual void setSymbol(options::SymbolRef,Property){throw options::Exception("Unimplemented",u"This live property owner does not support Symbol keys");}
    virtual void setOwnSymbol(options::SymbolRef,Property){throw options::Exception("Unimplemented",u"This live property owner does not support Symbol keys");}
    virtual void eraseSymbol(options::SymbolRef){throw options::Exception("Unimplemented",u"This live property owner does not support Symbol keys");}
    virtual std::vector<options::SymbolRef> symbolKeys()const{return {};}
    virtual std::vector<std::string> keys()const=0;
    // Full own-key enumeration is distinct from Object.keys/Object.assign.
    // Native Context projections need all descriptor keys, including
    // non-enumerable Symbols, while PropertyList::keys remains string-only.
    virtual std::vector<std::string> ownStringKeys()const{return keys();}
    virtual std::vector<options::SymbolRef> ownSymbolKeys()const{return symbolKeys();}
    virtual Property prototype()const=0;
    virtual Property primitive(options::PrimitiveHint)=0;
    // Source Object.assign copy semantics. Context-backed records override this
    // to preserve enumerable symbol keys as well as string keys.
    virtual PropertyList copyEnumerableOwn();
    // Exact Source object identity, available without reading properties.
    virtual std::optional<options::ObjectRef> sourceObjectIdentity()const noexcept{return {}; }
};
struct PropertyStorage {
    std::shared_ptr<PropertyRecordBinding> binding;
    std::vector<std::pair<std::string, Property>> entries;
    std::vector<std::pair<options::SymbolRef, Property>> symbolEntries;
    std::shared_ptr<PropertyStorage> prototype;
    bool objectPrototype = true;
    bool arrayPrototype = false;
    bool array = false;
    std::vector<Property> arrayValues;
    std::vector<bool> arrayPresent;
};

inline bool Property::operator==(const Property& other) const {
    const auto* left=std::get_if<std::shared_ptr<PropertyStorage>>(&value);
    const auto* right=std::get_if<std::shared_ptr<PropertyStorage>>(&other.value);
    if(left&&right){
        if(*left==*right)return true;
        if(!*left||!*right||!(*left)->binding||!(*right)->binding)return false;
        const auto leftIdentity=(*left)->binding->sourceObjectIdentity();
        const auto rightIdentity=(*right)->binding->sourceObjectIdentity();
        return leftIdentity&&rightIdentity&&*leftIdentity==*rightIdentity;
    }
    return value==other.value;
}

// Copies are handles to the same dictionary, like JS object references. Own
// entries are separate from the prototype; deletion never hides inherited data.
class PropertyList {
    friend struct Property;
    using Storage = PropertyStorage;
    std::shared_ptr<Storage> storage_ = std::make_shared<Storage>();
    explicit PropertyList(std::shared_ptr<Storage> storage):storage_(std::move(storage)){}
    static Property builtin(std::string_view name) {
        static const std::pair<std::string_view, BuiltinProperty> values[] = {
            {"constructor", BuiltinProperty::constructor},
            {"__defineGetter__", BuiltinProperty::defineGetter},
            {"__defineSetter__", BuiltinProperty::defineSetter},
            {"hasOwnProperty", BuiltinProperty::hasOwnProperty},
            {"__lookupGetter__", BuiltinProperty::lookupGetter},
            {"__lookupSetter__", BuiltinProperty::lookupSetter},
            {"isPrototypeOf", BuiltinProperty::isPrototypeOf},
            {"propertyIsEnumerable", BuiltinProperty::propertyIsEnumerable},
            {"toString", BuiltinProperty::toString},
            {"valueOf", BuiltinProperty::valueOf},
            {"toLocaleString", BuiltinProperty::toLocaleString},
            {"__proto__", BuiltinProperty::prototype}
        };
        for (auto [key, value] : values) if (key == name) return value;
        return Missing{};
    }
    static Property resolve(const std::shared_ptr<Storage>& storage, std::string_view name) {
        bool ordinary = true;
        for (auto p = storage; p; p = p->prototype)
        {
            if(p->binding)return p->binding->get(name);
            for (const auto& [key, value] : p->entries) if (key == name) return value;
            if(p->array){
                if(name=="length")return static_cast<double>(p->arrayValues.size());
                if(auto i=arrayIndex(name);i&&*i<p->arrayValues.size()&&p->arrayPresent[*i])return p->arrayValues[*i];
            }
            ordinary = p->objectPrototype;
            if(!p->prototype&&p->arrayPrototype&&ordinary){
                if(name=="length")return 0;
                if(name=="constructor")return BuiltinProperty::arrayConstructor;
                if(name=="toString")return BuiltinProperty::arrayToString;
                if(name=="join")return BuiltinProperty::arrayJoin;
            }
        }
        if (!ordinary) return Missing{};
        if (name == "__proto__" && storage->prototype) {
            Property value;
            value.value = storage->prototype;
            return value;
        }
        if(name=="__proto__"&&storage->arrayPrototype)return BuiltinProperty::arrayPrototype;
        return builtin(name);
    }
    static std::optional<std::uint32_t> arrayIndex(std::string_view key) {
        if (key.empty() || (key.size() > 1 && key.front() == '0')) return {};
        std::uint32_t n{};
        auto [p, e] = std::from_chars(key.data(), key.data() + key.size(), n);
        if (e != std::errc{} || p != key.data() + key.size() || n == std::numeric_limits<std::uint32_t>::max()) return {};
        return n;
    }
public:
    PropertyList() = default;
    static PropertyList live(std::shared_ptr<PropertyRecordBinding> binding,bool array=false){
        if(!binding)throw options::Exception("TypeError",u"Live Property record requires a binding");
        PropertyList result;result.storage_->binding=std::move(binding);
        result.storage_->array=array;return result;
    }
    static std::optional<PropertyList> object(const Property& value) {
        if (auto p = std::get_if<std::shared_ptr<Storage>>(&value.value); p && *p && !(*p)->array) {
            return PropertyList(*p);
        }
        return {};
    }
    static std::optional<PropertyList> record(const Property& value){
        if(auto p=std::get_if<std::shared_ptr<Storage>>(&value.value);p&&*p)return PropertyList(*p);return {};
    }
    PropertyList(std::initializer_list<std::pair<std::string, Property>> entries) {
        for (const auto& [key, value] : entries) setOwn(key, value);
    }
    static PropertyList withPrototype(const PropertyList& parent) {
        // PropertyStorage is already the canonical handle for either a native
        // record or a Context-backed record. Keeping that handle as the
        // prototype preserves Source Object.create semantics without copying
        // entries or manufacturing a second property object.
        PropertyList result;
        result.storage_->prototype = parent.storage_;
        return result;
    }
    static PropertyList withNullPrototype() {
        PropertyList result;
        result.storage_->objectPrototype=false;
        return result;
    }
    Property get(std::string_view name) const { return resolve(storage_, name); }
    Property getKey(const Property& name) const {
        if(const auto* symbol=std::get_if<options::SymbolRef>(&name.value)){
            for(auto current=storage_;current;current=current->prototype){
                if(current->binding){if(current->binding->hasOwnSymbol(*symbol))return current->binding->getOwnSymbol(*symbol);continue;}
                for(const auto& [key,value]:current->symbolEntries)if(key==*symbol)return value;
            }
            return Missing{};
        }
        if(const auto* text=std::get_if<std::string>(&name.value))return get(*text);
        throw options::Exception("TypeError",u"Property key must be converted with ToPropertyKey");
    }
    Property getOwn(std::string_view name) const {
        if(storage_->binding)return storage_->binding->getOwn(name);
        if(storage_->array){if(name=="length")return static_cast<double>(storage_->arrayValues.size());if(auto i=arrayIndex(name);i&&*i<storage_->arrayValues.size()&&storage_->arrayPresent[*i])return storage_->arrayValues[*i];}
        for (const auto& [key, value] : storage_->entries) if (key == name) return value;
        return Missing{};
    }
    bool hasOwn(std::string_view name) const {
        if(storage_->binding)return storage_->binding->hasOwn(name);
        if(storage_->array){if(name=="length")return true;if(auto i=arrayIndex(name);i&&*i<storage_->arrayValues.size()&&storage_->arrayPresent[*i])return true;}
        for (const auto& [key, value] : storage_->entries) if (key == name) return true;
        return false;
    }
    bool has(std::string_view name) const {
        bool ordinary = true;
        for (auto p = storage_; p; p = p->prototype)
        {
            if(p->binding)return p->binding->has(name);
            for (const auto& [key, value] : p->entries) if (key == name) return true;
            if(p->array){
                if(name=="length")return true;
                if(auto i=arrayIndex(name);i&&*i<p->arrayValues.size()&&p->arrayPresent[*i])return true;
            }
            if(!p->prototype&&p->objectPrototype&&p->arrayPrototype&&(name=="length"||name=="join"||name=="toString"||name=="constructor"))return true;
            ordinary = p->objectPrototype;
        }
        return ordinary && !builtin(name).missing();
    }
    void setOwn(std::string_view name, Property value) {
        if(storage_->binding){storage_->binding->setOwn(name,std::move(value));return;}
        if(storage_->array){
            if(name=="length"){
                auto n=std::get_if<double>(&value.value);
                if(!n||!std::isfinite(*n)||*n<0||*n>0xFFFFFFFFULL||std::trunc(*n)!=*n)throw options::Exception("RangeError",u"Invalid array length");
                storage_->arrayValues.resize(static_cast<std::size_t>(*n));storage_->arrayPresent.resize(static_cast<std::size_t>(*n),false);return;
            }
            if(auto i=arrayIndex(name)){
                if(*i>=storage_->arrayValues.size()){storage_->arrayValues.resize(std::size_t(*i)+1);storage_->arrayPresent.resize(std::size_t(*i)+1,false);}
                storage_->arrayValues[*i]=std::move(value);storage_->arrayPresent[*i]=true;return;
            }
        }
        for (auto& [key, old] : storage_->entries) if (key == name) {
            old = std::move(value); return;
        }
        storage_->entries.emplace_back(name, std::move(value));
    }
    void setOwnKey(const Property& name,Property value){
        if(const auto* symbol=std::get_if<options::SymbolRef>(&name.value)){
            if(storage_->binding){storage_->binding->setOwnSymbol(*symbol,std::move(value));return;}
            for(auto& [key,old]:storage_->symbolEntries)if(key==*symbol){old=std::move(value);return;}
            storage_->symbolEntries.emplace_back(*symbol,std::move(value));
            return;
        }
        if(const auto* text=std::get_if<std::string>(&name.value)){setOwn(*text,std::move(value));return;}
        throw options::Exception("TypeError",u"Property key must be converted with ToPropertyKey");
    }
    void set(std::string_view name, Property value) {
        if(storage_->binding){storage_->binding->set(name,std::move(value));return;}
        // The inherited Object.prototype __proto__ setter ignores scalar values.
        // An own data property on any layer instead allows a new own property.
        if (name == "__proto__") {
            bool data = false;
            bool ordinary = true;
            for (auto p = storage_; p; p = p->prototype)
            {
                if(p->binding)throw options::Exception("Unimplemented",u"Native Property __proto__ assignment through a live prototype requires descriptor binding");
                for (const auto& [key, v] : p->entries) if (key == name) data = true;
                ordinary = p->objectPrototype;
            }
            if (!data && ordinary) {
                if (auto parent = std::get_if<std::shared_ptr<Storage>>(&value.value)) {
                    if(*parent&&(*parent)->binding)throw options::Exception("Unimplemented",u"Mixed native/Context Property prototype requires a canonical receiver binding");
                    for (auto p = *parent; p; p = p->prototype)
                        if (p == storage_) throw std::invalid_argument("Cyclic __proto__ value");
                    storage_->prototype = *parent;
                    storage_->objectPrototype = true;
                    storage_->arrayPrototype = false;
                } else if (value.value == Property::Value(BuiltinProperty::prototype)) {
                    storage_->prototype.reset();
                    storage_->objectPrototype = true;
                    storage_->arrayPrototype = false;
                } else if (value.value == Property::Value(BuiltinProperty::arrayPrototype)) {
                    storage_->prototype.reset();
                    storage_->objectPrototype = true;
                    storage_->arrayPrototype = true;
                } else if (std::holds_alternative<PropertyNull>(value.value)) {
                    storage_->prototype.reset();
                    storage_->objectPrototype = false;
                    storage_->arrayPrototype = false;
                }
                return;
            }
        }
        setOwn(name, std::move(value));
    }
    void setKey(const Property& name,Property value){
        if(const auto* symbol=std::get_if<options::SymbolRef>(&name.value)){
            if(storage_->binding){storage_->binding->setSymbol(*symbol,std::move(value));return;}
            setOwnKey(name,std::move(value));
            return;
        }
        if(const auto* text=std::get_if<std::string>(&name.value)){set(*text,std::move(value));return;}
        throw options::Exception("TypeError",u"Property key must be converted with ToPropertyKey");
    }
    void erase(std::string_view name) {
        if(storage_->binding){storage_->binding->erase(name);return;}
        if(storage_->array){if(name=="length")return;if(auto i=arrayIndex(name);i&&*i<storage_->arrayValues.size()){storage_->arrayValues[*i]=Missing{};storage_->arrayPresent[*i]=false;}}
        std::erase_if(storage_->entries, [&](const auto& entry) { return entry.first == name; });
    }
    std::vector<std::string> keys() const {
        if(storage_->binding)return storage_->binding->keys();
        std::vector<std::string> result;
        if(storage_->array)for(std::size_t i=0;i<storage_->arrayValues.size();++i)if(storage_->arrayPresent[i])result.push_back(std::to_string(i));
        for (const auto& [key, value] : storage_->entries) result.push_back(key);
        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            auto i = arrayIndex(a), j = arrayIndex(b);
            if (i && j) return *i < *j;
            return bool(i) && !j;
        });
        return result;
    }
    void assign(const PropertyList& source) {
        for (const auto& key : source.keys()) set(key, source.getOwn(key));
        for (const auto& key : source.symbolKeys())setKey(Property(key),source.getKey(Property(key)));
    }
    std::vector<options::SymbolRef> symbolKeys()const{
        if(storage_->binding)return storage_->binding->symbolKeys();
        std::vector<options::SymbolRef> result;
        result.reserve(storage_->symbolEntries.size());
        for(const auto& [key,value]:storage_->symbolEntries)result.push_back(key);
        return result;
    }
    // Object.assign({}, source) as used by Tree.Node.copy: copy enumerable
    // own string keys and symbols from this same storage owner. Bound Context
    // records retain descriptors, getters and nested identities in place.
    PropertyList copyOwnEnumerableProperties()const{
        if(storage_->binding)return storage_->binding->copyEnumerableOwn();
        PropertyList result;
        const auto ownKeys=keys();
        for(const auto& key:ownKeys){
            if(hasOwn(key))result.set(key,getOwn(key));
        }
        for(const auto& key:symbolKeys())result.setOwnKey(Property(key),getKey(Property(key)));
        return result;
    }
};
inline PropertyList PropertyRecordBinding::copyEnumerableOwn(){
    PropertyList result;
    // Object.assign snapshots all own keys before running any getters, then
    // checks each current descriptor and performs Get/Set in key order.
    const auto stringKeys=ownStringKeys();
    const auto symbols=ownSymbolKeys();
    for(const auto& key:stringKeys)
        if(ownPropertyEnumerable(key))result.set(key,get(key));
    for(const auto& symbol:symbols)
        if(ownSymbolEnumerable(symbol))result.setKey(Property(symbol),getSymbol(symbol));
    return result;
}
inline Property::Property(const PropertyList& v) : value(v.storage_) {}

// Native array storage in the same Property graph as ordinary property objects.
// Numeric slots preserve holes versus own undefined; iteration reads live slots.
class PropertyArray {
    friend struct Property;
    std::shared_ptr<PropertyStorage> storage_=std::make_shared<PropertyStorage>();
    explicit PropertyArray(std::shared_ptr<PropertyStorage> storage):storage_(std::move(storage)){}
public:
    PropertyArray(){storage_->array=true;storage_->arrayPrototype=true;}
    PropertyArray(std::initializer_list<Property> values):PropertyArray(){for(const auto& p:values)push(p);}
    static std::optional<PropertyArray> array(const Property& p){
        if(auto v=std::get_if<std::shared_ptr<PropertyStorage>>(&p.value);v&&*v&&(*v)->array)return PropertyArray(*v);return {};
    }
    std::size_t size()const{
        if(storage_->binding)return static_cast<std::size_t>(std::get<double>(storage_->binding->get("length").value));
        return storage_->arrayValues.size();
    }
    bool has(std::size_t i)const{return storage_->binding?storage_->binding->hasOwn(std::to_string(i)):i<size()&&storage_->arrayPresent[i];}
    Property get(std::size_t i)const{return record().get(std::to_string(i));}
    PropertyList record()const{return PropertyList::record(Property(*this)).value();}
    void resize(std::size_t length){
        if(length>0xFFFFFFFFULL)throw options::Exception("RangeError",u"Invalid array length");
        if(storage_->binding){storage_->binding->set("length",Property(static_cast<double>(length)));return;}
        storage_->arrayValues.resize(length);storage_->arrayPresent.resize(length,false);
    }
    void set(std::size_t i,Property value){
        if(i>=0xFFFFFFFFULL)throw options::Exception("RangeError",u"Native array index exceeds index domain");
        if(storage_->binding){storage_->binding->set(std::to_string(i),std::move(value));return;}
        if(i>=size())resize(i+1);storage_->arrayValues[i]=std::move(value);storage_->arrayPresent[i]=true;
    }
    void push(Property value){set(size(),std::move(value));}
    void erase(std::size_t i){if(storage_->binding){storage_->binding->erase(std::to_string(i));return;}if(i<size()){storage_->arrayValues[i]=Missing{};storage_->arrayPresent[i]=false;}}
    std::vector<std::string> keys()const{return record().keys();}
    PropertyArray spread()const{PropertyArray out;for(std::size_t i=0;i<size();++i)out.push(get(i));return out;}
    // Native nonnegative index binding for Source slice(1) in MacroMap.
    // HasProperty/Get preserve inherited slots and holes; this is not spread.
    PropertyArray slice(std::size_t first=0)const;
};
inline Property::Property(const PropertyArray& v):value(v.storage_){}
}
