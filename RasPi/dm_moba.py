import json
import random
import time
import traceback

from layout_renderer import LayoutRenderer
from fia_control import FIA


def main():
    fia = FIA("/dev/ttyAMA1", (3, 0), width=480, height=128)
    renderer = LayoutRenderer("fonts", fia=fia)
    
    with open("layouts/departures_4x2_single_dm_moba.json", 'r') as f:
        layout_train = json.load(f)
    
    with open("layouts/info_text_4x2_single_dm_moba.json", 'r') as f:
        layout_info = json.load(f)
    
    print("Starting")
    print("Deleting all scroll buffers")
    for i in range(20):
        fia.delete_scroll_buffer(0x80 + i)

    while True:
        try:
            print("Showing train-style info page (German)")
            data = {
            'placeholders': {
                    'departure': "12:00",
                    'train': "ICE 705",
                    'info': "Fahrt fällt heute aus              +++",
                    'via': "Wernmünden \xb4 Thalmühle \xb4 Erlenbach",
                    'destination': "Modelleisenbahn",
                    'next_train_1_departure': "14:00",
                    'next_train_1_destination': "Modelleisenbahn",
                    'next_train_1_info': "fällt aus"
                }
            }
            renderer.free_scroll_buffers()
            renderer.display(layout_train, data)
            time.sleep(10)
            
            print("Showing train-style info page (English)")
            data = {
            'placeholders': {
                    'departure': "12:00",
                    'train': "ICE 705",
                    'info': "Today cancelled                   +++",
                    'via': "Wernmünden \xb4 Thalmühle \xb4 Erlenbach",
                    'destination': "Model railway",
                    'next_train_1_departure': "14:00",
                    'next_train_1_destination': "Model railway",
                    'next_train_1_info': "cancelled"
                }
            }
            renderer.free_scroll_buffers()
            renderer.display(layout_train, data)
            time.sleep(10)
            
            print("Showing animation")
            renderer.free_scroll_buffers()
            fia.send_gif("images/dm_moba_fia_anim.gif", num_loops=1) # ~5 seconds
            
            print("Showing info text (German)")
            data = {
            'placeholders': {
                    'text': "Liebe Besucher*innen,\n\naufgrund von Wartungsarbeiten müssen die Vorführungen der Modellbahn derzeit leider eine Pause einlegen.\nAb dem 29.04.2024 ist die Modellbahn wieder wie gewohnt für Sie da.\n\nWir bitten um Ihr Verständnis.\nIhr Modellbahnteam"
                }
            }
            renderer.free_scroll_buffers()
            renderer.display(layout_info, data)
            time.sleep(10)
            
            print("Showing info text (English)")
            data = {
            'placeholders': {
                    'text': "Dear Visitors,\n\ndue to construction work, the model railway is currently out of service.\n\nThe demonstrations will be back as usual from 29/04/2024.\n\nWe apologise for the inconvenience.\nThe model railway team."
                }
            }
            renderer.free_scroll_buffers()
            renderer.display(layout_info, data)
            time.sleep(10)
            
            print("Showing animation")
            renderer.free_scroll_buffers()
            fia.send_gif("images/dm_moba_fia_anim.gif", num_loops=1) # ~5 seconds
        except KeyboardInterrupt:
            return
        except:
            traceback.print_exc()
            print("Continuing in 10 seconds")
            time.sleep(10)


if __name__ == "__main__":
    main()
