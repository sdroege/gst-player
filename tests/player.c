/* GStreamer
 *
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <check.h>

#include <gst/player/gstplayer.h>

START_TEST (test_create_and_free)
{
  GstPlayer * player;

  player = gst_player_new ();
  fail_unless (player != NULL);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_set_and_get_uri)
{
  GstPlayer *player;
  gchar *uri;

  player = gst_player_new ();

  fail_unless (player != NULL);

  gst_player_set_uri (player, "/path/to/a/file");
  uri = gst_player_get_uri (player);

  fail_unless (g_strcmp0 (uri, "/path/to/a/file") == 0);

  g_free (uri);
  g_object_unref (player);
}

END_TEST;

static Suite *
player_suite (void)
{
  Suite *s = suite_create ("GstPlayer");

  TCase *tc_general = tcase_create ("general");

  tcase_add_test (tc_general, test_create_and_free);
  tcase_add_test (tc_general, test_set_and_get_uri);

  suite_add_tcase (s, tc_general);

  return s;
}

int
main (int argc, char **argv)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  gst_init (NULL, NULL);

  s = player_suite ();
  sr = srunner_create (s);

  srunner_run_all (sr, CK_NORMAL);

  number_failed = srunner_ntests_failed (sr);

  srunner_free (sr);
  return (number_failed == 0) ? 0 : -1;
}
