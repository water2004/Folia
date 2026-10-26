// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_direction;
import std;
export import folia.mermaid.elk_layout;

export namespace folia::mermaid {
struct ElkGraphPadding { double top=12,left=12,bottom=12,right=12; };
// GraphTransformer's default READING_DIRECTION coordinate normalization.
[[nodiscard]] ElkGraphPadding elk_normalize_padding(ElkGraphPadding padding,ElkLayoutDirection direction);
[[nodiscard]] ElkNodeBox elk_export_direction_box(ElkNodeBox box,double width,double height,ElkLayoutDirection direction);
// Measured INSIDE/V_TOP/H_CENTER title. The adapter supplies the frame padding
// separately; ELK's node-label padding defaults to five on each side.
struct ElkGroupTitle { double width=0,height=0,padding=5; };
struct ElkGroupLayoutBox { ElkGraphPadding padding; std::optional<ElkGroupTitle> title; };
[[nodiscard]] ElkGraphPadding elk_group_content_padding(const ElkGroupLayoutBox& box);
[[nodiscard]] ElkNodeBox elk_group_title_box(const ElkNodeBox& group,const ElkGroupTitle& title);
}
