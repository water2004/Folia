/*************************************************************
 *
 *  Copyright (c) 2017-2026 The MathJax Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *************************************************************/
/**
 * @file Native typed Node contracts based on MathJax core/Tree/Node.ts.
 */
export module folia.math.core.tree.node_types;
import std;
export import folia.math.core.tree.factory_types;

export namespace folia::math::tree {
template<class Product, class Kind> class Node;

template<class Product, class Kind>
class NodeOwner {
public:
    virtual ~NodeOwner() = default;
    virtual void setChildren(Product&, std::span<Product* const>) = 0;
    virtual void append(Product& parent, Product& child) = 0;
    virtual void replace(Product& parent, Product& oldChild, Product& replacement) = 0;
    virtual void remove(Product&) = 0;
    virtual Product& clone(Product const&, bool keepIds) = 0;
    virtual bool nodeIsKind(Product const&, Kind) const = 0;
    virtual std::string_view kindName(Kind) const = 0;
};

template<class Product, class Kind>
class Node : public FactoryNode<Kind> {
protected:
    Product* parent_{};
    std::vector<Product*> children_;
    virtual NodeOwner<Product, Kind>& treeOwner() const noexcept = 0;
public:
    Node() = default;
    virtual ~Node() = default;
    Node(Node const&) = delete;
    Node& operator=(Node const&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    Product* parent() const noexcept { return parent_; }
    std::span<Product* const> children() const noexcept { return children_; }

    virtual bool isKind(Kind) const = 0;
    virtual void setChildren(std::span<Product* const>) = 0;
    virtual Product& appendChild(Product&) = 0;
    virtual Product& replaceChild(Product& newChild, Product& oldChild) = 0;
    virtual Product& removeChild(Product&) = 0;
    virtual std::optional<std::size_t> childIndex(Product const&) const = 0;
    virtual Product& copy(bool keepIds = false) const = 0;
    virtual std::vector<Product*> findNodes(Kind) = 0;
    virtual void walkTree(std::function<void(Product&)> const&) = 0;
    virtual std::string toString() const = 0;
};
}
