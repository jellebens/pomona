"""Demeter — the pomona autodosing controller (cards #278/#224).

Named for the goddess of the harvest: she feeds the tower. Demeter watches
the reservoir chemistry over MQTT and corrects it with the calibrated
DFR0523 dosing pumps — pH-Down when the water drifts alkaline, nutrients
A then B when the EC sags — inside the same hard rails the interim Tethys
agent operated under. The firmware keeps the final veto (10 s hard cap per
command, one channel at a time).
"""

__version__ = "0.1.0"
