# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry import benchmark
from telemetry.core import discover
from telemetry import story

from measurements import skpicture_printer


def _MatchPageSetName(story_set_name, story_set_base_dir):
  story_sets = discover.DiscoverClasses(story_set_base_dir, story_set_base_dir,
                                        story.StorySet).values()
  for s in story_sets:
    if story_set_name == s.Name():
      return s
  return None


@benchmark.Disabled
class SkpicturePrinter(perf_benchmark.PerfBenchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--page-set-name',  action='store', type='string')
    parser.add_option('--page-set-base-dir', action='store', type='string')
    parser.add_option('-s', '--skp-outdir',
                      help='Output directory for the SKP files')
  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if not args.page_set_name:
      parser.error('Please specify --page-set-name')
    if not args.page_set_base_dir:
      parser.error('Please specify --page-set-base-dir')
    if not args.skp_outdir:
      parser.error('Please specify --skp-outdir')

  @classmethod
  def Name(cls):
    return 'skpicture_printer'

  def CreatePageTest(self, options):
    return skpicture_printer.SkpicturePrinter(options.skp_outdir)

  def CreateStorySet(self, options):
    story_set_class = _MatchPageSetName(options.page_set_name,
                                        options.page_set_base_dir)
    return story_set_class()
