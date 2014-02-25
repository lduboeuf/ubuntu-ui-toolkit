# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-
#
# Copyright (C) 2014 Canonical Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

import testtools

from ubuntuuitoolkit import scenarios


class ScenariosTestCase(testtools.TestCase):

    def test_get_nexus_4_scenario(self):
        expected_scenarios = [
            ('Simulating Nexus 4 in desktop',
             dict(app_width=768, app_height=1280, grid_unit_px=18)),
        ]
        test_scenarios = scenarios.get_device_simulation_scenarios(
            devices=scenarios.NEXUS4_DEVICE)
        self.assertEqual(expected_scenarios, test_scenarios)

    def test_get_nexus_10_scenario(self):
        expected_scenarios = [
            ('Simulating Nexus 10 in desktop',
             dict(app_width=2560, app_height=1600, grid_unit_px=20))
        ]
        test_scenarios = scenarios.get_device_simulation_scenarios(
            devices=scenarios.NEXUS10_DEVICE)
        self.assertEqual(expected_scenarios, test_scenarios)

    def test_get_default_scenarios_must_return_supported_devices(self):
        expected_scenarios = [
            ('Simulating Nexus 4 in desktop',
             dict(app_width=768, app_height=1280, grid_unit_px=18)),
            ('Simulating Nexus 10 in desktop',
             dict(app_width=2560, app_height=1600, grid_unit_px=20))
        ]
        test_scenarios = scenarios.get_device_simulation_scenarios()
        self.assertEqual(expected_scenarios, test_scenarios)
