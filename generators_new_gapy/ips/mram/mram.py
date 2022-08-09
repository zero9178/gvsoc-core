#
# Copyright (C) 2020 GreenWaves Technologies
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import gsystree as st
from devices.flash.flash import Flash

class Mram(Flash):

    def __init__(self, parent, name, size):

        super(Mram, self).__init__(parent, name)

        self.add_properties({
            'vp_component': 'pulp.mram.mram_v1_impl',
            'size': size
        })

        self.add_property('content/image', self.get_image_path())
        
        self.add_property('content/partitions/readfs/files', [])
        self.add_property('content/partitions/readfs/type', 'readfs')
        self.add_property('content/partitions/readfs/enabled', False)

        self.add_property('datasheet/type', 'mram')
        self.add_property('datasheet/size', '2MB')
        self.add_property('datasheet/block-size', '8KB')