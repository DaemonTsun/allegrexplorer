
#include "allegrex/disassemble.hpp"
#include "allegrexplorer_context.hpp"
#include "psp_module_info_window.hpp"

void psp_module_info_window()
{
    elf_psp_module *mod = &actx.disasm.psp_module;
    prx_sce_module_info *mod_info = &mod->module_info;

    if (ImGui::Begin("Module Info"))
    {
        ImGui::PushFont(actx.ui.fonts.mono);
        float char_width = actx.ui.fonts.mono->Glyphs['x'].AdvanceX;
        ImGui::PushItemWidth(PRX_MODULE_NAME_LEN * char_width);
        ImGui::InputText("Module Name", mod_info->name, PRX_MODULE_NAME_LEN, ImGuiInputTextFlags_ReadOnly);

        int attr = mod_info->attribute;
        ImGui::InputInt("Attributes", &attr, 0, 0, ImGuiInputTextFlags_ReadOnly
                                                 | ImGuiInputTextFlags_CharsHexadecimal);


        u32 v1 = mod_info->version[0];
        ImGui::InputScalar("Version 1", ImGuiDataType_U32, &v1, nullptr, nullptr, U32_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);

        u32 v2 = mod_info->version[1];
        ImGui::InputScalar("Version 2", ImGuiDataType_U32, &v2, nullptr, nullptr, U32_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        
        ImGui::InputScalar("gp", ImGuiDataType_U32, &mod_info->gp, nullptr, nullptr, VADDR_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        
        ImGui::InputScalar("Export start", ImGuiDataType_U32, &mod_info->export_offset_start,
                           nullptr, nullptr, VADDR_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        ImGui::InputScalar("Export end", ImGuiDataType_U32, &mod_info->export_offset_end,
                           nullptr, nullptr, VADDR_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        
        ImGui::InputScalar("Import start", ImGuiDataType_U32, &mod_info->import_offset_start,
                           nullptr, nullptr, VADDR_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        ImGui::InputScalar("Import end", ImGuiDataType_U32, &mod_info->import_offset_end,
                           nullptr, nullptr, VADDR_FORMAT,
                           ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();
        ImGui::PopFont();
    }

    ImGui::End();
}
