"""Capture the live Third Person view after its walking test."""
import unreal as u
world=u.EditorLevelLibrary.get_pie_worlds(False)[0]
u.SystemLibrary.execute_console_command(world,'HighResShot 2560x1440 filename="E:/Repos/mythic-forge/Assets/_render/RenderTests/Gothic/City/city_play.png"')
