import os
import re
from pathlib import Path
from typing import Dict, Optional, List

SKILLS_DIR = Path(__file__).parent.parent / "skills"

class SkillManager:
    """
    Manages loading and injection of external skill-based prompts (personas).
    Skills are stored in server/skills/<skill-name>/SKILL.md
    """
    def __init__(self):
        self.skills: Dict[str, str] = {}
        self._load_all_skills()

    def _load_all_skills(self):
        if not SKILLS_DIR.exists():
            return

        for skill_folder in SKILLS_DIR.iterdir():
            if skill_folder.is_dir():
                skill_md = skill_folder / "SKILL.md"
                if skill_md.exists():
                    try:
                        content = skill_md.read_text(encoding="utf-8")
                        # Strip YAML frontmatter if present
                        cleaned_content = re.sub(r'^---.*?---', '', content, flags=re.DOTALL).strip()
                        self.skills[skill_folder.name] = cleaned_content
                    except Exception as e:
                        print(f"Error loading skill {skill_folder.name}: {e}")

    def get_skill_prompt(self, skill_name: str) -> Optional[str]:
        """Returns the prompt content for a given skill name."""
        return self.skills.get(skill_name)

    def list_skills(self) -> List[str]:
        """Returns a list of available skill names."""
        return list(self.skills.keys())

    def inject_skill(self, skill_name: str, base_system_prompt: str) -> str:
        """Injects a skill's prompt into a base system prompt."""
        skill_prompt = self.get_skill_prompt(skill_name)
        if not skill_prompt:
            return base_system_prompt
        
        return f"{base_system_prompt}\n\n### LOADED SKILL: {skill_name.upper()}\n{skill_prompt}"

# Global instance for easy access
skill_manager = SkillManager()
